"""B's dirty-pending extension to the in-memory proposed A hooks only.

The old clean candidate stays frozen at Core89a75b24. New source requires this
extension; it never edits or imports A's working public files.
"""


def extend(text):
    def replace(name, before, after, count=1):
        name = 'src/gpgpu-sim/' + name
        assert text[name].count(before) == count, (name, before[:100], text[name].count(before))
        text[name] = text[name].replace(before, after)

    h = 'gpu-cache.h'
    replace(h, '#include "ep-l2-tvd-directory.h"', '#include "ep-l2-tvd-directory.h"\n#include "ep-l2-tvd-bank-identity.h"')
    replace(h, '  void set_rg1_enabled(bool enabled)', '''  void set_tvd_identity(bool enabled) {
    assert(!enabled || (banked() && m_ft_mode==2 && m_bank_ops==1 && !m_rg1));
    m_tvd_identity=enabled;
  }
  bool tvd_identity_enabled() const { return m_tvd_identity; }
  void set_rg1_enabled(bool enabled)''')
    replace(h, '  bool m_ideal_wb, m_static_s64;', '  bool m_ideal_wb, m_static_s64;\n  bool m_tvd_identity=false;')
    replace(h, 'if (op.r == r && op.id == id && op.write == write) {',
            '''if (ep_l2::tvd_bank_request_matches(op,r,id,write,client,token,
          m_slots[global_id].generation,m_tvd_identity)) {''')
    replace(h, 'if (m_pending[bank][i].r == r && m_pending[bank][i].id == id &&\n          m_pending[bank][i].write == write) pending_index = i;',
            '''if (ep_l2::tvd_bank_request_matches(m_pending[bank][i],r,id,write,
          client,token,m_slots[global_id].generation,m_tvd_identity)) pending_index=i;''')
    replace(h, '      pending_index = m_pending[bank].size() - 1;', '''      pending_index = m_pending[bank].size() - 1;
      if (m_tvd_identity) {
        auto &p=m_pending[bank][pending_index];
        p.client=client; p.token=token; p.generation=m_slots[global_id].generation;
        // Native response/fill/lower/frontend heads, no unbounded new queue.
        assert(pending_operations()<=4);
      }''')
    replace(h, '    if(m_rg1){cancel_request(h,false);cancel_request(h,true);}',
            '    if(m_rg1 || m_tvd_identity){cancel_request(h,false);cancel_request(h,true);}')
    replace('ep-l2-rg1.inc', '  if(!m_rg1)return;', '  if(!m_rg1 && !m_tvd_identity)return;')
    replace('ep-l2-rg1.inc',
            'if((*q)[i].client!=GENERIC && (*q)[i].token==token){++m_rg_cancel[(*q)[i].client];q->erase(q->begin()+i);}',
            'if((*q)[i].client!=GENERIC && (*q)[i].token==token){if(m_rg1)++m_rg_cancel[(*q)[i].client];q->erase(q->begin()+i);}')
    replace(h, '    m_ep_l2_payload.set_rg1_enabled(config.m_ep_l2_rg1);',
            '    m_ep_l2_payload.set_rg1_enabled(config.m_ep_l2_rg1);\n    m_ep_l2_payload.set_tvd_identity(config.m_ep_l2_tvd_dirty);')
    replace(h, 'CLEAN_ONLY or DIRTY_INFLIGHT_ONLY; combined modes unqualified',
            'CLEAN_ONLY or DIRTY_PENDING_INFLIGHT; combined modes unqualified')

    # The immutable packet is already accepted, but a victim reader's queued
    # bank operation still uses the WB role. Keep that role until its real read
    # grant is consumed and response pin is retired; directory state may advance.
    replace('ep-l2-ft-cache.inc',
            '''    if (m_tvd->at(t.index).payload.valid())
      m_ep_l2_payload.transfer(it->second.handle,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_INFLIGHT);''',
            '''    if (m_tvd->at(t.index).payload.valid()) {
      if (!m_ep_l2_payload.inspect(it->second.handle).pins)
        m_ep_l2_payload.transfer(it->second.handle,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_INFLIGHT);
      // Else retain WB role while a paid reader pin protects its retry key.
    }''')
    replace('gpu-cache.cc',
            '    if (h.valid()) m_ep_l2_payload.transfer(h,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_VALID);',
            '''    if (h.valid() && !m_ep_l2_payload.inspect(h).pins)
      m_ep_l2_payload.transfer(h,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_VALID);
    // A pinned read completes independently; response commit finishes the role
    // transition. Its payload generation and queued logical identity stay live.''')
    replace('gpu-sim.cc', 'study DIRTY_INFLIGHT_ONLY (not pending-read forwarding)',
            'study DIRTY_PENDING_INFLIGHT (new source, native qualification required)')
