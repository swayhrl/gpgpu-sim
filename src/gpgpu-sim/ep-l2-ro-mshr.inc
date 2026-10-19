// Included once in gpu-cache.cc after mshr_table declarations.
// Uses the existing native line table and D512 free-list only.
unsigned mshr_table::ro_targets_on_line(new_addr_type line) const {
  if (!m_ro_descriptor_count) return 0;
  unsigned count = 0;
  for (unsigned i = 0; i < m_descriptor_pool.size(); ++i)
    if (m_descriptor_pool[i].m_ro_external &&
        m_descriptor_pool[i].m_ro_line == line) ++count;
  return count;
}

unsigned mshr_table::targets_on_line(new_addr_type line) const {
  table::const_iterator entry = m_data.find(line);
  return ro_targets_on_line(line) +
         (entry == m_data.end() ? 0 : entry->second.m_list.size());
}

bool mshr_table::reserve_ro_descriptor(
    new_addr_type line, mem_fetch *mf, const mem_access_sector_mask_t &sectors,
    ro_descriptor_handle &result) {
  result = ro_descriptor_handle();
  if (!m_descriptor_pool_size || !mf || sectors.none() ||
      targets_on_line(line) >= m_descriptor_per_line_cap ||
      m_free_descriptor_ids.empty()) return false;
  for (unsigned n = 0; n < m_descriptor_pool.size(); ++n)
    if (m_descriptor_pool[n].m_mf == mf) return false;
  const unsigned id = m_free_descriptor_ids.back();
  ep_l2_descriptor &d = m_descriptor_pool[id];
  assert(!d.m_mf && !d.m_ro_external);
  // Never permit a generation wrap to turn a stale descriptor into a live one.
  assert(d.m_ro_generation != ~0ULL);
  ++d.m_ro_generation;
  d.m_mf = mf; d.m_sectors = sectors; d.m_ro_external = true;
  d.m_ro_line = line; d.m_response_queued = false;
  m_free_descriptor_ids.pop_back();
  ++m_ro_descriptor_count;
  result = ro_descriptor_handle(id, d.m_ro_generation);
  return true;
}

bool mshr_table::ro_descriptor_live(const ro_descriptor_handle &h) const {
  return h.valid() && h.id < m_descriptor_pool.size() &&
         m_descriptor_pool[h.id].m_ro_external &&
         m_descriptor_pool[h.id].m_ro_generation == h.generation;
}

void mshr_table::release_ro_descriptor(const ro_descriptor_handle &h) {
  assert(ro_descriptor_live(h));
  ep_l2_descriptor &d = m_descriptor_pool[h.id];
  const unsigned long long generation = d.m_ro_generation;
  d = ep_l2_descriptor();
  d.m_ro_generation = generation;
  --m_ro_descriptor_count;
  m_free_descriptor_ids.push_back(h.id);
}

bool mshr_table::reserve_ro_line(new_addr_type line) {
  if (m_data.find(line) != m_data.end() || m_data.size() >= m_num_entries)
    return false;
  m_data[line].m_ro_line_owner = true;
  return true;
}

void mshr_table::bind_ro_line(new_addr_type line, ep_l2::PayloadHandle h) {
  assert(m_data.at(line).m_ro_line_owner); m_data.at(line).m_ro_handle=h;
}
bool mshr_table::lookup_ro_line(new_addr_type line, ep_l2::PayloadHandle &h) const {
  auto it=m_data.find(line);
  if(it==m_data.end() || !it->second.m_ro_line_owner)return false;
  h=it->second.m_ro_handle;return true;
}

void mshr_table::release_ro_line(new_addr_type line) {
  table::iterator e = m_data.find(line);
  assert(e != m_data.end() && e->second.m_ro_line_owner);
  assert(e->second.m_pending_sectors.none() && ro_targets_on_line(line) == 0);
  e->second.m_ro_line_owner = false;
  if (e->second.m_list.empty()) m_data.erase(e);
}

void mshr_table::ro_line_issue(new_addr_type line,
                              const mem_access_sector_mask_t &mask) {
  table::iterator e = m_data.find(line);
  assert(e != m_data.end() && e->second.m_ro_line_owner);
  assert((mask & e->second.m_issued_sectors).none());
  e->second.m_issued_sectors |= mask;
  e->second.m_pending_sectors |= mask;
}

void mshr_table::ro_line_ready(new_addr_type line,
                              const mem_access_sector_mask_t &mask) {
  table::iterator e = m_data.find(line);
  assert(e != m_data.end() && e->second.m_ro_line_owner);
  assert((mask & ~e->second.m_pending_sectors).none());
  e->second.m_pending_sectors &= ~mask;
  e->second.m_ready_sectors |= mask;
}

