#!/usr/bin/env python3
"""Produce, never apply, B's public hooks against exact Core84359d4 for A.

Historical TVD is reused selectively. Current atomic masks/completion, Tag
victim identity binding, quota, RG1 and bypass fixes remain in their parent.
No simulator compiler or full application is started by this tool.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

BASE = '84359d4ba492acb639a2092fa271e9169038bf7d'
OLD = 'be5dcbe1fa5b9a501e81a77a6c399e6f6530e536'
FILES = ['src/gpgpu-sim/' + p for p in ('gpu-cache.h', 'gpu-cache.cc', 'gpu-sim.cc', 'l2cache.cc', 'ep-l2-ft-cache.inc')]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[3]
    def blob(commit, name):
        return subprocess.check_output(['git', 'show', commit + ':' + name], cwd=root).decode()
    original = {name: blob(BASE, name) for name in FILES}
    text = dict(original)
    def replace(name, before, after, count=1):
        full = 'src/gpgpu-sim/' + name
        assert text[full].count(before) == count, (name, before[:100], text[full].count(before))
        text[full] = text[full].replace(before, after)
    h = 'gpu-cache.h'
    replace(h, '#include "ep-l2-ft-interface.h"', '#include <memory>\n#include "ep-l2-ft-interface.h"\n#include "ep-l2-tvd-directory.h"')
    replace(h, '    m_ep_l2_feature_tvd = false;', '    m_ep_l2_feature_tvd = false;\n    m_ep_l2_tvd_clean = m_ep_l2_tvd_dirty = false;')
    replace(h, '  bool m_ep_l2_feature_tvd;', '  bool m_ep_l2_feature_tvd;\n  bool m_ep_l2_tvd_clean, m_ep_l2_tvd_dirty;')
    replace(h, 'ep_l2_needs_lower_read(false) {}', 'ep_l2_needs_lower_read(false), ep_l2_tvd_candidate(false) {}')
    replace(h, '  bool ep_l2_needs_lower_read;', '  bool ep_l2_needs_lower_read;\n  bool ep_l2_tvd_candidate;')
    old_header = blob(OLD, 'src/gpgpu-sim/gpu-cache.h')
    bind = old_header[old_header.index('  void tvd_bind('):old_header.index('\n protected:', old_header.index('  void tvd_bind('))]
    replace(h, '  void inc_dirty() { m_dirty++; }', '  void inc_dirty() { m_dirty++; }\n' + bind)
    replace(h, 'enum role { RESIDENT, BYPASS, WB, IDEAL_REFERENCE };', 'enum role { RESIDENT, BYPASS, WB, IDEAL_REFERENCE, TVD };')
    replace(h, '    BYPASS_READ\n', '    BYPASS_READ, TVD_READ\n')
    replace(h, 'BYPASS_READY, WB_RESERVED, WB_STAGED\n', 'BYPASS_READY, WB_RESERVED, WB_STAGED, TVD_VALID, TVD_INFLIGHT\n')
    replace(h, 'm_bank_wb_readout(0), m_bank_bypass_fill(0), m_bank_bypass_read(0),', 'm_bank_wb_readout(0), m_bank_bypass_fill(0), m_bank_bypass_read(0), m_bank_tvd_read(0),')
    replace(h, '  unsigned free_slots() const { return physical_slots() - used(RESIDENT) - used(BYPASS) - used(WB); }', '  unsigned tvd_used() const { return used(TVD); }\n  unsigned free_slots() const { return physical_slots() - used(RESIDENT) - used(BYPASS) - used(WB) - used(TVD); }')
    replace(h, '      } else if (s.allocation_role == WB) {', '''      } else if (s.allocation_role == TVD) {
        if (!m_ft_mode || (s.status != TVD_VALID && s.status != TVD_INFLIGHT) ||
            !owners.insert(s.owner).second) return false;
      } else if (s.allocation_role == WB) {''')
    replace(h, '      case BYPASS_READ: ++m_bank_bypass_read; break;', '      case BYPASS_READ: ++m_bank_bypass_read; break;\n      case TVD_READ: ++m_bank_tvd_read; break;')
    replace(h, '      m_bank_bypass_read;\n', '      m_bank_bypass_read, m_bank_tvd_read;\n')
    replace(h, '    m_ft_last_wb_versions.fill(0);', '''    m_ft_last_wb_versions.fill(0);
    m_tvd_lookup = NULL; m_tvd_lookup_cycle = 0;
    m_tvd_query_cycle = (unsigned long long)-1; m_tvd_lookup_done = false;
    m_tvd_lookup_wait = m_tvd_wb_wait = m_tvd_bank_wait = m_tvd_resp_wait = m_tvd_lookup_queries = 0;
    if (config.m_ep_l2_tvd_clean || config.m_ep_l2_tvd_dirty)
      m_tvd.reset(new ep_l2_tvd_directory<>(128, config.m_ep_l2_tvd_clean, config.m_ep_l2_tvd_dirty));''')
    replace(h, '  bool ft_enabled() const { return m_config.m_ep_l2_ft_mode != 0; }', '''  typedef ep_l2_tvd_directory<> tvd_directory;
  bool tvd_enabled() const { return bool(m_tvd); }
  bool tvd_write_candidate(mem_fetch *mf) const {
    return m_tvd && m_tvd_lookup == mf && m_tvd_lookup_done &&
        m_tvd->live(m_tvd_lookup_ticket) && (mf->get_is_write() || mf->isatomic());
  }
  enum tvd_frontend_result { TVD_NORMAL, TVD_WAIT, TVD_RESPONSE };
  tvd_frontend_result tvd_frontend(mem_fetch *, unsigned long long, bool);
  void tvd_response_commit(mem_fetch *, unsigned long long);
  void tvd_epoch_cleanup(unsigned long long);
  void flush() { tvd_epoch_cleanup(ft_physical_cycle()); data_cache::flush(); }
  void invalidate() { tvd_epoch_cleanup(ft_physical_cycle()); data_cache::invalidate(); }
  void tvd_print(FILE *, unsigned);
  const tvd_directory *tvd_for_test() const { return m_tvd.get(); }
  unsigned long long tvd_actual_lower_read_packets() const { return m_ft_stats.lower_read_packets; }
  unsigned long long tvd_actual_lower_read_bytes() const { return m_ft_stats.lower_read_bytes; }
  unsigned long long tvd_lookup_queries_for_test() const { return m_tvd_lookup_queries; }
  bool ft_enabled() const { return m_config.m_ep_l2_ft_mode != 0; }''')
    replace(h, '!m_ft_quota.reserved && !m_ft_quota.live); }', '!m_ft_quota.reserved && !m_ft_quota.live &&\n      (!m_tvd || (m_tvd->epoch_ready() && !m_tvd_read))); }')
    replace(h, 'unsigned ep_l2_wad_occupancy() const { return m_ep_l2_wad_entries_live.size(); }', 'unsigned ep_l2_wad_occupancy() const { return m_tvd ? m_tvd->mandatory() : m_ep_l2_wad_entries_live.size(); }')
    replace(h, '  bool ep_l2_wad_contains(new_addr_type block_addr) const {', '  bool ep_l2_wad_contains(new_addr_type block_addr) const {\n    if (m_tvd) { auto t=m_tvd->find(block_addr); return m_tvd->live(t) && m_tvd->at(t.index).mandatory(); }')
    replace(h, '  void ft_rollback();', '''  void ft_rollback();
  bool tvd_promote(mem_fetch *, unsigned, std::list<cache_event> &);
  bool tvd_install_victim(new_addr_type, ep_l2::PayloadHandle, unsigned, unsigned, bool, unsigned long long);
  bool tvd_reclaim_one(unsigned, unsigned long long);
  unsigned long long m_gpu_cycle_for_tvd() const;
  std::unique_ptr<tvd_directory> m_tvd;
  std::unique_ptr<tvd_directory::read_lease> m_tvd_read;
  mem_fetch *m_tvd_lookup;
  bool m_tvd_lookup_done;
  tvd_directory::ticket m_tvd_lookup_ticket;
  unsigned long long m_tvd_lookup_cycle, m_tvd_query_cycle;
  unsigned long long m_tvd_lookup_wait, m_tvd_wb_wait, m_tvd_bank_wait, m_tvd_resp_wait, m_tvd_lookup_queries;''')
    replace(h, 'bool dirty, wad, landing_reserved, bound, quota_reserved;', 'bool dirty, wad, landing_reserved, bound, quota_reserved, retain_clean;')
    replace(h, 'landing_reserved(false), bound(false), quota_reserved(false) {', 'landing_reserved(false), bound(false), quota_reserved(false), retain_clean(false) {')
    replace(h, 'observed_cycles, staging_occupancy_sum, descriptor_responses;', 'observed_cycles, staging_occupancy_sum, descriptor_responses, lower_read_packets, lower_read_bytes;')
    replace(h, 'staging_occupancy_sum(0), descriptor_responses(0) {}', 'staging_occupancy_sum(0), descriptor_responses(0), lower_read_packets(0), lower_read_bytes(0) {}')
    replace(h, '  void validate_ep_l2_mode_config() const {', '''  void validate_ep_l2_mode_config() const {
    if ((m_config.m_ep_l2_tvd_clean || m_config.m_ep_l2_tvd_dirty) &&
        (m_config.m_ep_l2_ft_mode != 2 || m_config.m_ep_l2_wbuf_entries != 16 ||
         m_config.m_ep_l2_wad_entries != 128 || m_config.m_ep_l2_bank_ops != 1 ||
         m_config.m_ep_l2_mq2 || m_config.m_ep_l2_rg1 || m_config.m_ep_l2_static_s64 ||
         m_config.m_ep_l2_wb_local_cap || m_config.m_ep_l2_ideal_wb ||
         m_config.m_ep_l2_ideal_meta || m_config.m_ep_l2_ideal_read_hit)) {
      fprintf(stderr,"TVD study supports finite1168 Unified P1, CLEAN_ONLY or DIRTY_INFLIGHT_ONLY; combined modes unqualified\\n"); abort();
    }''')

    c = 'gpu-cache.cc'
    replace(c, '#include "ep-l2-ft-cache.inc"', '#include "ep-l2-ft-cache.inc"\n#include "ep-l2-tvd-cache.inc"')
    replace(c, '  const size_t erased = m_ep_l2_wad_entries_live.erase(normalized);\n  assert(erased == 1);', '''  if (m_tvd) {
    auto t=m_tvd->find(normalized); assert(m_tvd->live(t));
    const auto h=m_tvd->at(t.index).payload;
    assert(m_tvd->set_done(t,cycle)==tvd_directory::OK);
    if (h.valid()) m_ep_l2_payload.transfer(h,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_VALID);
  } else {
    const size_t erased=m_ep_l2_wad_entries_live.erase(normalized); assert(erased==1);
  }''')
    replace(c, '  new_addr_type block_addr = m_config.block_addr(addr);\n  if (ep_l2_wad_enabled() && plan.is_read', '  new_addr_type block_addr = m_config.block_addr(addr);\n  if (tvd_write_candidate(mf)) { plan.ep_l2_tvd_candidate=true; plan.exact=false; return; }\n  if (ep_l2_wad_enabled() && plan.is_read')
    replace(c, 'm_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries &&', '(m_tvd ? m_tvd->preview_install(m_tag_array->get_block(cache_index)->m_block_addr,true).status != tvd_directory::OK : m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries) &&')

    f = 'ep-l2-ft-cache.inc'
    # Host-alias retirement is the narrowly scoped historical correction.
    # Other parent functions are left intact, notably atomic completion labels.
    old_ft = blob(OLD, 'src/gpgpu-sim/' + f)
    start = original['src/gpgpu-sim/' + f].index('bool l2_cache::ft_try_force_tag_access(')
    end = original['src/gpgpu-sim/' + f].index('\nvoid l2_cache::descriptor_admitted(', start)
    os = old_ft.index('bool l2_cache::ft_try_force_tag_access(')
    oe = old_ft.index('\nvoid l2_cache::descriptor_admitted(', os)
    replace(f, original['src/gpgpu-sim/' + f][start:end], old_ft[os:oe])
    replace(f, '    assert(m_ep_l2_wad_entries_live.erase(p.victim_address) == 1);', '''    if (m_tvd) assert(m_tvd->rollback_staged(m_tvd->find(p.victim_address), ft_physical_cycle())==tvd_directory::OK);
    else assert(m_ep_l2_wad_entries_live.erase(p.victim_address)==1);''')
    replace(f, '  const unsigned long long physical_cycle = ft_physical_cycle();\n  m_ep_l2_last_payload_request_result = ep_l2_payload_store::GRANTED;\n  const new_addr_type owner = m_config.block_addr(addr);', '  const unsigned long long physical_cycle = ft_physical_cycle();\n  m_ep_l2_last_payload_request_result = ep_l2_payload_store::GRANTED;\n  const new_addr_type owner = m_config.block_addr(addr);\n  if (tvd_write_candidate(mf) && !tvd_promote(mf,time,events)) return RESERVATION_FAIL;')
    replace(f, 'if (dirty && m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries)', 'if (dirty && (m_tvd ? m_tvd->preview_install(m_tag_array->get_block(index)->m_block_addr,true).status != tvd_directory::OK : m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries))')
    replace(f, '    if (m_config.m_ep_l2_ft_mode == 2 && (dirty || !live)) {', '''    bool retain_clean = false;
    if (m_tvd && m_config.m_ep_l2_tvd_clean && live && !dirty &&
        m_ep_l2_payload.inspect(victim).ready_sector_mask.any() &&
        m_tvd->preview_install(m_ep_l2_payload.inspect(victim).owner,false).status==tvd_directory::OK) {
      landing=m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT,index%4);
      retain_clean=landing.valid();
    }
    if (m_config.m_ep_l2_ft_mode == 2 && (dirty || !live)) {''')
    replace(f, '      if (!landing.valid()) { ++m_ft_stats.pool_full;', '''      if (!landing.valid() && m_tvd && tvd_reclaim_one(index%4,physical_cycle))
        landing=m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT,index%4);
      if (!landing.valid()) { ++m_ft_stats.pool_full;''')
    replace(f, 'p.tag_index = index; p.dirty = dirty; p.victim', 'p.tag_index = index; p.dirty = dirty; p.retain_clean=retain_clean; p.victim')
    replace(f, '      assert(m_ep_l2_wad_entries_live.insert(p.victim_address).second);', '''      if (m_tvd) {
        const auto dirty_mask=m_tag_array->get_block(index)->get_dirty_sector_mask().to_ulong();
        const auto ready_mask=p.saved.ready_sector_mask.to_ulong();
        const bool retain=m_config.m_ep_l2_tvd_dirty && (dirty_mask & ~ready_mask)==0;
        assert(tvd_install_victim(p.victim_address,retain?victim:ep_l2::PayloadHandle(),
            retain?ready_mask:dirty_mask,dirty_mask,true,physical_cycle));
      } else assert(m_ep_l2_wad_entries_live.insert(p.victim_address).second);''')
    replace(f, '  if (replacement && m_ft_pending.dirty) assert(m_ft_pending.bound);', '''  if (replacement && m_ft_pending.dirty) assert(m_ft_pending.bound);
  if (replacement && m_ft_pending.retain_clean) {
    auto &p=m_ft_pending;
    if (tvd_install_victim(p.saved.owner,p.victim,p.saved.ready_sector_mask.to_ulong(),0,false,physical_cycle))
      m_ep_l2_payload.transfer(p.victim,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_VALID);
    else m_ep_l2_payload.release(p.victim);
  }''')
    replace(f, '  ++m_ft_stats.accepted;\n  m_tag_array->l2_char_tracking_refresh_line(index);', '  if (m_tvd_lookup==mf) m_tvd_lookup=NULL;\n  ++m_ft_stats.accepted;\n  m_tag_array->l2_char_tracking_refresh_line(index);')
    replace(f, 'void l2_cache::lower_accepted(mem_fetch *mf, unsigned long long cycle) {', '''void l2_cache::lower_accepted(mem_fetch *mf, unsigned long long cycle) {
  if (ft_enabled() && !mf->get_is_write()) {
    ++m_ft_stats.lower_read_packets; m_ft_stats.lower_read_bytes+=mf->get_data_size();
  }''')
    replace(f, '  m_ep_l2_payload.release(it->second.handle);', '''  if (m_tvd) {
    const auto t=m_tvd->find(m_config.block_addr(mf->get_addr()));
    assert(m_tvd->lower_accept(t,true,cycle)==tvd_directory::OK);
    if (m_tvd->at(t.index).payload.valid())
      m_ep_l2_payload.transfer(it->second.handle,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_INFLIGHT);
    else m_ep_l2_payload.release(it->second.handle);
  } else m_ep_l2_payload.release(it->second.handle);''')
    replace(f, '      m_ft_stats.staging_occupancy_sum, m_ft_stats.descriptor_responses);', '      m_ft_stats.staging_occupancy_sum, m_ft_stats.descriptor_responses);\n  if (m_tvd) const_cast<l2_cache *>(this)->tvd_print(fp,slice);')

    c = 'l2cache.cc'
    marker = '  // New L2 texture accesses and/or non-texture accesses.'
    hook = '''  bool tvd_frontend_handled=false;
  if (!m_config->m_L2_config.disabled() && m_L2cache->tvd_enabled() && !m_icnt_L2_queue->empty()) {
    auto mf=m_icnt_L2_queue->top(); const auto now=m_gpu->gpu_sim_cycle+m_gpu->gpu_tot_sim_cycle;
    const bool eligible=!m_config->m_L2_texure_only || mf->istexture();
    const auto result=eligible?m_L2cache->tvd_frontend(mf,now,!m_L2_icnt_queue->full()):l2_cache::TVD_NORMAL;
    tvd_frontend_handled=result!=l2_cache::TVD_NORMAL;
    if (result==l2_cache::TVD_RESPONSE) {
      mf->set_reply(); mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE,now);
      ft_record_admission(mf); ep_l2_m0a_record_response_enqueue(); ft_record_response(mf);
      m_L2_icnt_queue->push(mf); m_L2cache->tvd_response_commit(mf,now); m_icnt_L2_queue->pop();
    }
  }

'''
    replace(c, marker, hook + marker)
    replace(c, '  if (!m_icnt_L2_queue->empty()) {\n    mem_fetch *mf = m_icnt_L2_queue->top();', '  if (!tvd_frontend_handled && !m_icnt_L2_queue->empty()) {\n    mem_fetch *mf = m_icnt_L2_queue->top();')
    replace(c, '      l2_admission_inputs admission;\n      if (plan.exact)', '      l2_admission_inputs admission;\n      if (plan.ep_l2_tvd_candidate) { admit=!m_L2_icnt_queue->full(); }\n      else if (plan.exact)')
    c = 'gpu-sim.cc'
    replace(c, '  option_parser_register(opp, "-gpgpu_ep_l2_feature_tvd", OPT_BOOL,', '''  option_parser_register(opp,"-gpgpu_ep_l2_tvd_clean",OPT_BOOL,&m_L2_config.m_ep_l2_tvd_clean,"study CLEAN_ONLY", "0");
  option_parser_register(opp,"-gpgpu_ep_l2_tvd_dirty",OPT_BOOL,&m_L2_config.m_ep_l2_tvd_dirty,"study DIRTY_INFLIGHT_ONLY (not pending-read forwarding)", "0");
  option_parser_register(opp, "-gpgpu_ep_l2_feature_tvd", OPT_BOOL,''')
    patch = ''.join('diff --git a/{0} b/{0}\n'.format(name) + ''.join(difflib.unified_diff(original[name].splitlines(True), text[name].splitlines(True), fromfile='a/'+name, tofile='b/'+name)) for name in FILES if original[name] != text[name])
    # Git accepts an empty context line without a redundant space prefix;
    # preserve exact blank source context without embedding trailing blanks.
    patch=patch.replace('\n \n','\n\n')
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / 'A_PUBLIC_HOOKS.patch').write_text(patch)
    manifest = dict(base_core=BASE, historical_module=OLD, public_files_mutated=False,
                    patch_applied=False, compiled=False, production_tests_run=False,
                    patch_sha256=hashlib.sha256(patch.encode()).hexdigest(),
                    files={name: dict(base_sha256=hashlib.sha256(original[name].encode()).hexdigest(),
                                     proposed_sha256=hashlib.sha256(text[name].encode()).hexdigest()) for name in FILES})
    (a.out / 'ADAPTER_MANIFEST.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(dict(status='TEXTUAL_ADAPTER_GENERATED_NOT_COMPILED', public_files=len(FILES), patch_bytes=len(patch))))


if __name__ == '__main__':
    main()
