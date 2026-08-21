#include "l2_latebind_stats.h"

#include <algorithm>
#include <assert.h>

#include "gpu-cache.h"

std::vector<l2_latebind_stats *> l2_latebind_stats::s_instances;

l2_latebind_stats::l2_latebind_stats(const char *cache_name,
                                      unsigned subpartition_id)
    : m_cache_name(cache_name), m_subpartition_id(subpartition_id) {
  for (unsigned i = 0; i < 5; ++i) m_probe[i] = 0;
  for (unsigned i = 0; i < 3; ++i) m_accept[i] = 0;
  m_mshr_new = 0;
  m_mshr_merge = 0;
  m_mshr_entry_full = 0;
  m_mshr_merge_full = 0;
  m_reservation_alloc = 0;
  m_reservation_fill = 0;
  m_reservation_direct_ready = 0;
  m_fill_without_reservation = 0;
  m_sample_cycles = 0;
  m_resident_sector_cycles = 0;
  m_reserved_sector_cycles = 0;
  m_dirty_sector_cycles = 0;
  m_resident_sector_max = 0;
  m_reserved_sector_max = 0;
  m_dirty_sector_max = 0;
  m_data_port_busy_cycles = 0;
  m_fill_port_busy_cycles = 0;
  m_writeback_enqueued = 0;
  m_writeback_lower_accepted = 0;
  m_writeback_bytes = 0;
  m_writeback_sectors = 0;
  m_writeback_queue_cycles = 0;
  m_writeback_queue_max = 0;
  m_unresolved_requests = 0;
  m_unresolved_reservations = 0;
  s_instances.push_back(this);
}

l2_latebind_stats::~l2_latebind_stats() {
  std::vector<l2_latebind_stats *>::iterator it =
      std::find(s_instances.begin(), s_instances.end(), this);
  if (it != s_instances.end()) s_instances.erase(it);
}

unsigned long long l2_latebind_stats::lifetime_bucket(
    unsigned long long cycles) {
  if (cycles <= 255) return cycles;
  unsigned long long bucket = 256;
  while (bucket <= cycles / 2) bucket <<= 1;
  return bucket;
}

void l2_latebind_stats::record_probe(unsigned status) {
  if (status < 5) m_probe[status]++;
}

void l2_latebind_stats::record_accept(mem_fetch *mf, unsigned long long time,
                                      unsigned probe_status,
                                      unsigned access_status) {
  if (access_status == RESERVATION_FAIL) return;
  assert(mf);
  request_record record;
  record.accept_time = time;
  record.probe_status = probe_status;
  record.access_status = access_status;
  std::pair<request_map::iterator, bool> result =
      m_requests.insert(std::make_pair(mf, record));
  // A request is offered to L2 at most once after it has been accepted.
  assert(result.second);
  unsigned cls = probe_status == HIT ? 0 :
                 probe_status == HIT_RESERVED ? 1 : 2;
  m_accept[cls]++;
}

void l2_latebind_stats::record_latency(mem_fetch *mf,
                                       unsigned long long time) {
  request_map::iterator it = m_requests.find(mf);
  if (it == m_requests.end()) return;
  assert(time >= it->second.accept_time);
  unsigned cls = it->second.probe_status == HIT ? 0 :
                 it->second.probe_status == HIT_RESERVED ? 1 : 2;
  m_request_latency[cls][lifetime_bucket(time - it->second.accept_time)]++;
  m_requests.erase(it);
}

void l2_latebind_stats::record_reply(mem_fetch *mf, unsigned long long time) {
  record_latency(mf, time);
}

void l2_latebind_stats::record_consumed(mem_fetch *mf,
                                        unsigned long long time) {
  record_latency(mf, time);
}

void l2_latebind_stats::record_mshr(bool hit, bool available) {
  if (hit && available)
    m_mshr_merge++;
  else if (!hit && available)
    m_mshr_new++;
  else if (hit)
    m_mshr_merge_full++;
  else
    m_mshr_entry_full++;
}

void l2_latebind_stats::record_reservation(unsigned cache_index,
                                           unsigned sector_mask,
                                           unsigned long long time) {
  for (unsigned sector = 0; sector < SECTOR_CHUNCK_SIZE; ++sector) {
    if (!(sector_mask & (1u << sector))) continue;
    unsigned long long key =
        (static_cast<unsigned long long>(cache_index) << 8) | sector;
    m_reservation_start[key] = time;
    m_reservation_alloc++;
  }
}

void l2_latebind_stats::record_fill(unsigned cache_index, unsigned sector_mask,
                                    unsigned long long time) {
  for (unsigned sector = 0; sector < SECTOR_CHUNCK_SIZE; ++sector) {
    if (!(sector_mask & (1u << sector))) continue;
    unsigned long long key =
        (static_cast<unsigned long long>(cache_index) << 8) | sector;
    std::map<unsigned long long, unsigned long long>::iterator it =
        m_reservation_start.find(key);
    if (it == m_reservation_start.end()) {
      m_fill_without_reservation++;
      continue;
    }
    assert(time >= it->second);
    m_reservation_lifetime[lifetime_bucket(time - it->second)]++;
    m_reservation_start.erase(it);
    m_reservation_fill++;
  }
}

void l2_latebind_stats::record_reservation_ready(unsigned cache_index,
                                                 unsigned sector_mask,
                                                 unsigned long long time) {
  for (unsigned sector = 0; sector < SECTOR_CHUNCK_SIZE; ++sector) {
    if (!(sector_mask & (1u << sector))) continue;
    unsigned long long key =
        (static_cast<unsigned long long>(cache_index) << 8) | sector;
    std::map<unsigned long long, unsigned long long>::iterator it =
        m_reservation_start.find(key);
    if (it == m_reservation_start.end()) continue;
    assert(time >= it->second);
    m_reservation_lifetime[lifetime_bucket(time - it->second)]++;
    m_reservation_start.erase(it);
    m_reservation_direct_ready++;
  }
}

void l2_latebind_stats::sample(unsigned resident_sectors,
                               unsigned reserved_sectors,
                               unsigned dirty_sectors, bool data_port_busy,
                               bool fill_port_busy) {
  m_sample_cycles++;
  m_resident_sector_cycles += resident_sectors;
  m_reserved_sector_cycles += reserved_sectors;
  m_dirty_sector_cycles += dirty_sectors;
  m_resident_sector_max = std::max<unsigned long long>(
      m_resident_sector_max, resident_sectors);
  m_reserved_sector_max = std::max<unsigned long long>(
      m_reserved_sector_max, reserved_sectors);
  m_dirty_sector_max = std::max<unsigned long long>(m_dirty_sector_max,
                                                      dirty_sectors);
  if (data_port_busy) m_data_port_busy_cycles++;
  if (fill_port_busy) m_fill_port_busy_cycles++;
  m_writeback_queue_cycles += m_writebacks.size();
  m_writeback_queue_max = std::max<unsigned long long>(
      m_writeback_queue_max, m_writebacks.size());
}

void l2_latebind_stats::record_writeback_enqueue(mem_fetch *mf,
                                                  unsigned long long time,
                                                  unsigned bytes,
                                                  unsigned sectors) {
  writeback_record record;
  record.enqueue_time = time;
  record.bytes = bytes;
  record.sectors = sectors;
  std::pair<writeback_map::iterator, bool> result =
      m_writebacks.insert(std::make_pair(mf, record));
  assert(result.second);
  m_writeback_enqueued++;
  m_writeback_bytes += bytes;
  m_writeback_sectors += sectors;
}

void l2_latebind_stats::record_lower_request(mem_fetch *mf,
                                              unsigned long long time) {
  writeback_map::iterator it = m_writebacks.find(mf);
  if (it == m_writebacks.end()) return;
  assert(time >= it->second.enqueue_time);
  m_writeback_lower_accepted++;
  m_writebacks.erase(it);
}

void l2_latebind_stats::print_histogram(FILE *fp, const char *name,
                                        const histogram &hist) {
  for (histogram::const_iterator it = hist.begin(); it != hist.end(); ++it)
    fprintf(fp, "latebind_l2_hist name=%s bucket_cycles=%llu count=%llu\n",
            name, it->first, it->second);
}

void l2_latebind_stats::print(FILE *fp) const {
  fprintf(fp,
          "latebind_l2 subpartition=%u cache=%s probes_hit=%llu "
          "probes_hit_reserved=%llu probes_miss=%llu "
          "probes_reservation_fail=%llu probes_sector_miss=%llu "
          "accept_hit=%llu accept_merged=%llu accept_miss=%llu "
          "mshr_new=%llu mshr_merge=%llu mshr_entry_full=%llu "
          "mshr_merge_full=%llu reservation_alloc=%llu "
          "reservation_fill=%llu reservation_direct_ready=%llu "
          "fill_without_reservation=%llu "
          "sample_cycles=%llu resident_sector_cycles=%llu "
          "reserved_sector_cycles=%llu dirty_sector_cycles=%llu "
          "resident_sector_max=%llu reserved_sector_max=%llu "
          "dirty_sector_max=%llu data_port_busy_cycles=%llu "
          "fill_port_busy_cycles=%llu wb_enqueued=%llu "
          "wb_lower_accepted=%llu wb_bytes=%llu wb_sectors=%llu "
          "wb_queue_cycles=%llu wb_queue_max=%llu unresolved_requests=%zu "
          "unresolved_reservations=%zu\n",
          m_subpartition_id, m_cache_name.c_str(), m_probe[HIT],
          m_probe[HIT_RESERVED], m_probe[MISS], m_probe[RESERVATION_FAIL],
          m_probe[SECTOR_MISS], m_accept[0], m_accept[1], m_accept[2],
          m_mshr_new, m_mshr_merge, m_mshr_entry_full, m_mshr_merge_full,
          m_reservation_alloc, m_reservation_fill, m_reservation_direct_ready,
          m_fill_without_reservation,
          m_sample_cycles, m_resident_sector_cycles, m_reserved_sector_cycles,
          m_dirty_sector_cycles, m_resident_sector_max, m_reserved_sector_max,
          m_dirty_sector_max, m_data_port_busy_cycles, m_fill_port_busy_cycles,
          m_writeback_enqueued, m_writeback_lower_accepted, m_writeback_bytes,
          m_writeback_sectors, m_writeback_queue_cycles, m_writeback_queue_max,
          m_requests.size(), m_reservation_start.size());
  print_histogram(fp, "reservation_lifetime", m_reservation_lifetime);
  print_histogram(fp, "request_latency_hit", m_request_latency[0]);
  print_histogram(fp, "request_latency_merged", m_request_latency[1]);
  print_histogram(fp, "request_latency_miss", m_request_latency[2]);
}

void l2_latebind_stats::add_histogram(histogram &dst,
                                      const histogram &src) {
  for (histogram::const_iterator it = src.begin(); it != src.end(); ++it)
    dst[it->first] += it->second;
}

void l2_latebind_stats::print_global(FILE *fp) {
  unsigned long long totals[24] = {0};
  histogram reservation_lifetime;
  histogram request_latency[3];
  for (std::vector<l2_latebind_stats *>::const_iterator it =
           s_instances.begin();
       it != s_instances.end(); ++it) {
    const l2_latebind_stats &s = **it;
    totals[0] += s.m_probe[HIT];
    totals[1] += s.m_probe[HIT_RESERVED];
    totals[2] += s.m_probe[MISS];
    totals[3] += s.m_probe[RESERVATION_FAIL];
    totals[4] += s.m_probe[SECTOR_MISS];
    totals[5] += s.m_mshr_new;
    totals[6] += s.m_mshr_merge;
    totals[7] += s.m_mshr_entry_full;
    totals[8] += s.m_mshr_merge_full;
    totals[9] += s.m_reservation_alloc;
    totals[10] += s.m_reservation_fill;
    totals[23] += s.m_reservation_direct_ready;
    totals[11] += s.m_resident_sector_cycles;
    totals[12] += s.m_reserved_sector_cycles;
    totals[13] += s.m_dirty_sector_cycles;
    totals[14] = std::max(totals[14], s.m_resident_sector_max);
    totals[15] = std::max(totals[15], s.m_reserved_sector_max);
    totals[16] = std::max(totals[16], s.m_dirty_sector_max);
    totals[17] += s.m_data_port_busy_cycles;
    totals[18] += s.m_fill_port_busy_cycles;
    totals[19] += s.m_writeback_enqueued;
    totals[20] += s.m_writeback_lower_accepted;
    totals[21] += s.m_writeback_bytes;
    totals[22] += s.m_writeback_sectors;
    add_histogram(reservation_lifetime, s.m_reservation_lifetime);
    for (unsigned cls = 0; cls < 3; ++cls)
      add_histogram(request_latency[cls], s.m_request_latency[cls]);
  }
  fprintf(fp,
          "latebind_l2_global subpartitions=%zu probes_hit=%llu "
          "probes_hit_reserved=%llu probes_miss=%llu "
          "probes_reservation_fail=%llu probes_sector_miss=%llu "
          "mshr_new=%llu mshr_merge=%llu mshr_entry_full=%llu "
          "mshr_merge_full=%llu reservation_alloc=%llu "
          "reservation_fill=%llu reservation_direct_ready=%llu "
          "resident_sector_cycles=%llu "
          "reserved_sector_cycles=%llu dirty_sector_cycles=%llu "
          "resident_sector_max=%llu reserved_sector_max=%llu "
          "dirty_sector_max=%llu data_port_busy_cycles=%llu "
          "fill_port_busy_cycles=%llu wb_enqueued=%llu "
          "wb_lower_accepted=%llu wb_bytes=%llu wb_sectors=%llu\n",
          s_instances.size(), totals[0], totals[1], totals[2], totals[3],
          totals[4], totals[5], totals[6], totals[7], totals[8], totals[9],
          totals[10], totals[23], totals[11], totals[12], totals[13], totals[14],
          totals[15], totals[16], totals[17], totals[18], totals[19],
          totals[20], totals[21], totals[22]);
  print_histogram(fp, "global_reservation_lifetime", reservation_lifetime);
  print_histogram(fp, "global_request_latency_hit", request_latency[0]);
  print_histogram(fp, "global_request_latency_merged", request_latency[1]);
  print_histogram(fp, "global_request_latency_miss", request_latency[2]);
}
