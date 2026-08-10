#include "decoupled-l2-cache.h"

#include <assert.h>
#include <string.h>

#include "gpu-sim.h"
#include "l2cache.h"
#include "mem_fetch.h"

decoupled_l2_cache::decoupled_l2_cache(
    const char *name, l2_cache_config &cache_config,
    const memory_config *memory_config, mem_fetch_interface *memport,
    mem_fetch_allocator *mf_allocator)
    : m_name(name),
      m_cache_config(cache_config),
      m_memory_config(memory_config),
      m_memport(memport),
      m_mf_allocator(mf_allocator),
      m_next_token(0),
      m_bank_ready(memory_config->decoupled_l2_banks, 0),
      m_accesses(0),
      m_hits(0),
      m_misses(0),
      m_aad_merges(0),
      m_otf_reads(0),
      m_writes(0),
      m_writebacks(0),
      m_atomic_requests(0),
      m_token_stalls(0),
      m_aad_stalls(0),
      m_bank_stalls(0),
      m_bank_ops(memory_config->decoupled_l2_banks, 0) {
  assert(m_memory_config->decoupled_l2_req_entries > 0);
  assert(m_memory_config->decoupled_l2_aad_entries > 0);
  assert(m_memory_config->decoupled_l2_wbq_entries > 0);
  assert(!m_bank_ready.empty());
}

bool decoupled_l2_cache::fixed_mode() const {
  return m_memory_config->use_fixed_l2();
}

unsigned decoupled_l2_cache::bank_for(new_addr_type line) const {
  return (line / m_cache_config.get_line_sz()) % m_bank_ready.size();
}

bool decoupled_l2_cache::line_hit(new_addr_type line) const {
  return m_lines.find(line) != m_lines.end();
}

bool decoupled_l2_cache::request_is_write(mem_fetch *mf) const {
  // Atomics are completed through the normal GPGPU-Sim response path.  Treat
  // them as reads for residency, then let memory_sub_partition::pop() perform
  // the existing functional atomic callback exactly once.
  return mf->is_write() && !mf->isatomic();
}

bool decoupled_l2_cache::data_port_free() const {
  return m_requests.size() < m_memory_config->decoupled_l2_req_entries;
}

enum cache_request_status decoupled_l2_cache::access(
    new_addr_type addr, mem_fetch *mf, unsigned long long time,
    std::list<cache_event> &events) {
  if (!data_port_free()) {
    ++m_token_stalls;
    return RESERVATION_FAIL;
  }
  assert(mf);
  assert(m_token_for_mf.find(mf) == m_token_for_mf.end());

  request req;
  req.token = m_next_token++;
  req.mf = mf;
  req.line = m_cache_config.block_addr(addr);
  req.write = request_is_write(mf);
  req.atomic = mf->isatomic();
  m_requests[req.token] = req;
  m_token_for_mf[mf] = req.token;
  ++m_accesses;
  if (req.write) {
    ++m_writes;
    // memory_sub_partition's baseline write-allocate shortcut must not reply
    // early: this backend owns the response and will return this mf later.
    events.push_back(cache_event(WRITE_ALLOCATE_SENT));
  }
  if (req.atomic) ++m_atomic_requests;

  if (fixed_mode()) {
    schedule_response(req.token, time + m_memory_config->decoupled_l2_fixed_latency);
  } else {
    m_tag_queue.push_back(req.token);
  }
  assert_unique_state();
  return MISS;
}

void decoupled_l2_cache::schedule_response(unsigned token,
                                           unsigned long long ready_time) {
  m_scheduled_responses.push_back(scheduled_response(ready_time, token));
}

void decoupled_l2_cache::retire_ready_responses(unsigned long long time) {
  while (!m_scheduled_responses.empty() &&
         m_scheduled_responses.front().ready_time <= time) {
    unsigned token = m_scheduled_responses.front().token;
    m_scheduled_responses.pop_front();
    std::map<unsigned, request>::const_iterator req = m_requests.find(token);
    assert(req != m_requests.end());
    m_response_ready.push_back(req->second.mf);
  }
}

void decoupled_l2_cache::process_tag(unsigned token, unsigned long long time) {
  std::map<unsigned, request>::iterator req_it = m_requests.find(token);
  assert(req_it != m_requests.end());
  request &req = req_it->second;

  std::map<new_addr_type, aad_entry>::iterator active = m_aad.find(req.line);
  if (active != m_aad.end()) {
    active->second.tokens.push_back(token);
    ++m_aad_merges;
    return;
  }

  if (line_hit(req.line)) {
    m_lines[req.line].last_touch = time;
    if (req.write) m_lines[req.line].dirty = true;
    ++m_hits;
    schedule_response(token, time + m_memory_config->decoupled_l2_hit_latency);
    return;
  }

  // Writes allocate and become dirty without a data read: data content is not
  // represented by this timing model.  Reads/atomics create one line OTF.
  if (req.write) {
    if (victim_requires_wbq() &&
        m_wbq.size() >= m_memory_config->decoupled_l2_wbq_entries) {
      ++m_aad_stalls;
      m_tag_queue.push_front(token);
      return;
    }
    ++m_misses;
    install_line(req.line, time);
    m_lines[req.line].dirty = true;
    schedule_response(token, time + m_memory_config->decoupled_l2_hit_latency);
    return;
  }

  if (m_aad.size() >= m_memory_config->decoupled_l2_aad_entries) {
    ++m_aad_stalls;
    m_tag_queue.push_front(token);
    return;
  }
  ++m_misses;
  aad_entry entry;
  entry.tokens.push_back(token);
  entry.lower_mf = req.mf;
  m_aad[req.line] = entry;
  m_lower_read_queue.push_back(req.line);
  ++m_otf_reads;
}

bool decoupled_l2_cache::victim_requires_wbq() const {
  if (m_lines.size() < m_cache_config.get_num_lines()) return false;
  for (std::map<new_addr_type, line_state>::const_iterator it = m_lines.begin();
       it != m_lines.end(); ++it) {
    if (it->second.dirty) return true;
  }
  return false;
}

bool decoupled_l2_cache::fill_port_free() const {
  return !victim_requires_wbq() ||
         m_wbq.size() < m_memory_config->decoupled_l2_wbq_entries;
}

void decoupled_l2_cache::enqueue_writeback(new_addr_type line,
                                           unsigned long long time) {
  assert(m_wbq.size() < m_memory_config->decoupled_l2_wbq_entries);
  wbq_entry entry;
  entry.line = line;
  entry.mf = m_mf_allocator->alloc(line, L2_WRBK_ACC,
                                   m_cache_config.get_line_sz(), true, time, 0);
  m_wbq.push_back(entry);
  m_writeback_mfs.insert(entry.mf);
  ++m_writebacks;
}

void decoupled_l2_cache::install_line(new_addr_type line,
                                      unsigned long long time) {
  if (!line_hit(line) && m_lines.size() >= m_cache_config.get_num_lines()) {
    std::map<new_addr_type, line_state>::iterator victim = m_lines.begin();
    for (std::map<new_addr_type, line_state>::iterator it = m_lines.begin();
         it != m_lines.end(); ++it) {
      if (it->second.last_touch < victim->second.last_touch) victim = it;
    }
    if (victim->second.dirty) enqueue_writeback(victim->first, time);
    m_lines.erase(victim);
  }
  line_state &state = m_lines[line];
  state.last_touch = time;
}

void decoupled_l2_cache::issue_lower_reads(unsigned long long time) {
  const unsigned attempts = m_lower_read_queue.size();
  std::set<unsigned> used_banks;
  for (unsigned i = 0; i < attempts; ++i) {
    new_addr_type line = m_lower_read_queue.front();
    m_lower_read_queue.pop_front();
    std::map<new_addr_type, aad_entry>::iterator active = m_aad.find(line);
    assert(active != m_aad.end());
    if (active->second.lower_issued) continue;
    const unsigned bank = bank_for(line);
    if (used_banks.count(bank) || m_bank_ready[bank] > time) {
      ++m_bank_stalls;
      m_lower_read_queue.push_back(line);
      continue;
    }
    if (m_memport->full(m_cache_config.get_line_sz(), false)) {
      m_lower_read_queue.push_front(line);
      return;
    }
    active->second.lower_issued = true;
    m_fill_waiters[active->second.lower_mf] = line;
    m_memport->push(active->second.lower_mf);
    // The front-end has one lower-read issue slot per bank.  Different banks
    // can proceed in one L2 cycle; same-bank requests remain queued.
    used_banks.insert(bank);
    m_bank_ready[bank] = time + 1;
    ++m_bank_ops[bank];
  }
}

void decoupled_l2_cache::issue_writebacks(unsigned long long time) {
  for (std::deque<wbq_entry>::iterator it = m_wbq.begin(); it != m_wbq.end();
       ++it) {
    if (it->issued) continue;
    if (m_memport->full(m_cache_config.get_line_sz(), true)) return;
    it->issued = true;
    m_memport->push(it->mf);
    const unsigned bank = bank_for(it->line);
    m_bank_ready[bank] = time + 1;
    ++m_bank_ops[bank];
    return;
  }
}

void decoupled_l2_cache::cycle(unsigned long long time) {
  if (!fixed_mode()) {
    const unsigned queued = m_tag_queue.size();
    std::set<unsigned> used_banks;
    for (unsigned i = 0; i < queued; ++i) {
      unsigned token = m_tag_queue.front();
      m_tag_queue.pop_front();
      std::map<unsigned, request>::const_iterator req = m_requests.find(token);
      assert(req != m_requests.end());
      unsigned bank = bank_for(req->second.line);
      if (used_banks.count(bank) || m_bank_ready[bank] > time) {
        ++m_bank_stalls;
        m_tag_queue.push_back(token);
        continue;
      }
      used_banks.insert(bank);
      m_bank_ready[bank] = time + m_memory_config->decoupled_l2_tag_latency;
      ++m_bank_ops[bank];
      process_tag(token, time + m_memory_config->decoupled_l2_tag_latency);
    }
    issue_lower_reads(time);
    issue_writebacks(time);
  }
  retire_ready_responses(time);
  assert_unique_state();
}

bool decoupled_l2_cache::waiting_for_fill(mem_fetch *mf) const {
  return m_fill_waiters.find(mf) != m_fill_waiters.end();
}

void decoupled_l2_cache::fill(mem_fetch *mf, unsigned long long time) {
  std::map<mem_fetch *, new_addr_type>::iterator fill = m_fill_waiters.find(mf);
  assert(fill != m_fill_waiters.end());
  const new_addr_type line = fill->second;
  std::map<new_addr_type, aad_entry>::iterator active = m_aad.find(line);
  assert(active != m_aad.end());

  install_line(line, time);
  for (std::vector<unsigned>::const_iterator token = active->second.tokens.begin();
       token != active->second.tokens.end(); ++token) {
    std::map<unsigned, request>::const_iterator req = m_requests.find(*token);
    assert(req != m_requests.end());
    if (req->second.write) m_lines[line].dirty = true;
    schedule_response(*token, time + m_memory_config->decoupled_l2_fill_latency);
  }
  m_fill_waiters.erase(fill);
  m_aad.erase(active);
  const unsigned bank = bank_for(line);
  m_bank_ready[bank] = time + 1;
  ++m_bank_ops[bank];
}

mem_fetch *decoupled_l2_cache::next_access() {
  assert(!m_response_ready.empty());
  mem_fetch *mf = m_response_ready.front();
  m_response_ready.pop_front();
  std::map<mem_fetch *, unsigned>::iterator token = m_token_for_mf.find(mf);
  assert(token != m_token_for_mf.end());
  m_requests.erase(token->second);
  m_token_for_mf.erase(token);
  return mf;
}

void decoupled_l2_cache::writeback_done(mem_fetch *mf) {
  if (m_writeback_mfs.erase(mf) == 0) return;
  for (std::deque<wbq_entry>::iterator it = m_wbq.begin(); it != m_wbq.end();
       ++it) {
    if (it->mf == mf) {
      m_wbq.erase(it);
      return;
    }
  }
  assert(0 && "writeback acknowledgement was not in WBQ");
}

void decoupled_l2_cache::force_tag_access(new_addr_type addr, unsigned,
                                          mem_access_sector_mask_t) {
  line_state &line = m_lines[m_cache_config.block_addr(addr)];
  line.dirty = false;
}

void decoupled_l2_cache::flush() { invalidate(); }

void decoupled_l2_cache::invalidate() {
  assert(m_aad.empty());
  assert(m_fill_waiters.empty());
  m_lines.clear();
}

void decoupled_l2_cache::print(FILE *fp, unsigned &accesses,
                               unsigned &misses) const {
  accesses += m_accesses;
  misses += m_misses;
  fprintf(fp,
          "decoupled_l2[%s]: access=%llu hit=%llu miss=%llu aad_merge=%llu "
          "otf=%llu write=%llu wb=%llu atomic=%llu token_stall=%llu "
          "aad_stall=%llu bank_stall=%llu\n",
          m_name.c_str(), m_accesses, m_hits, m_misses, m_aad_merges,
          m_otf_reads, m_writes, m_writebacks, m_atomic_requests,
          m_token_stalls, m_aad_stalls, m_bank_stalls);
}

void decoupled_l2_cache::display_state(FILE *fp) const {
  fprintf(fp,
          "decoupled_l2[%s]: access=%llu hit=%llu miss=%llu aad_merge=%llu "
          "otf=%llu write=%llu wb=%llu atomic=%llu token_stall=%llu "
          "aad_stall=%llu bank_stall=%llu req=%zu tag=%zu aad=%zu fill=%zu "
          "response=%zu wbq=%zu lines=%zu banks=",
          m_name.c_str(), m_accesses, m_hits, m_misses, m_aad_merges,
          m_otf_reads, m_writes, m_writebacks, m_atomic_requests,
          m_token_stalls, m_aad_stalls, m_bank_stalls, m_requests.size(),
          m_tag_queue.size(), m_aad.size(), m_fill_waiters.size(),
          m_response_ready.size(), m_wbq.size(), m_lines.size());
  for (unsigned bank = 0; bank < m_bank_ops.size(); ++bank)
    fprintf(fp, "%s%llu", bank ? "," : "", m_bank_ops[bank]);
  fprintf(fp, "\n");
}

void decoupled_l2_cache::assert_unique_state() const {
  assert(m_requests.size() == m_token_for_mf.size());
  for (std::map<mem_fetch *, unsigned>::const_iterator it = m_token_for_mf.begin();
       it != m_token_for_mf.end(); ++it) {
    std::map<unsigned, request>::const_iterator req = m_requests.find(it->second);
    assert(req != m_requests.end() && req->second.mf == it->first);
  }
  std::set<unsigned> aad_tokens;
  for (std::map<new_addr_type, aad_entry>::const_iterator it = m_aad.begin();
       it != m_aad.end(); ++it) {
    assert(!it->second.tokens.empty());
    std::set<unsigned> seen_tokens;
    for (std::vector<unsigned>::const_iterator token = it->second.tokens.begin();
         token != it->second.tokens.end(); ++token) {
      assert(seen_tokens.insert(*token).second);
      assert(aad_tokens.insert(*token).second);
      assert(m_requests.find(*token) != m_requests.end());
    }
    if (it->second.lower_issued) {
      assert(it->second.lower_mf);
      std::map<mem_fetch *, new_addr_type>::const_iterator fill =
          m_fill_waiters.find(it->second.lower_mf);
      assert(fill != m_fill_waiters.end() && fill->second == it->first);
    }
  }
}
