// Copyright (c) 2009-2021, Tor M. Aamodt, Vijay Kandiah, Nikos Hardavellas,
// Mahmoud Khairy, Junrui Pan, Timothy G. Rogers
// The University of British Columbia, Northwestern University, Purdue
// University All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer;
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution;
// 3. Neither the names of The University of British Columbia, Northwestern
//    University nor the names of their contributors may be used to
//    endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef MC_PARTITION_INCLUDED
#define MC_PARTITION_INCLUDED

#include "../abstract_hardware_model.h"
#include "dram.h"
#include "l2_admission_rules.h"
#include "l2-char-stats.h"

#include <list>
#include <map>
#include <queue>
#include <set>

class mem_fetch;

struct l2_block_episode_state {
  l2_block_episode_state()
      : ever_blocked_mask(0), prev_blocked_mask(0), first_block_cycle(0),
        total_block_cycles(0) {}
  unsigned long long ever_blocked_mask;
  unsigned long long prev_blocked_mask;
  unsigned long long first_block_cycle;
  unsigned long long total_block_cycles;
};

struct l2_char_stats {
  l2_char_stats() { clear(); }
  void clear();
  void sample(unsigned mshr_entries, unsigned mshr_targets,
              unsigned mshr_ready_entries, unsigned mshr_ready_targets,
              unsigned missq, unsigned missq_demand, unsigned missq_wb,
              unsigned l2_dramq, unsigned dram_l2q, unsigned l2_icntq,
              unsigned icnt_l2q, unsigned rop, bool data_busy, bool fill_busy,
              bool missq_full, bool dram_l2_full, bool l2_icnt_full,
              bool icnt_l2_full);
  void record_blockers(mem_fetch *mf, unsigned long long blocker_mask,
                       unsigned long long cycle,
                       std::map<mem_fetch *, l2_block_episode_state> &state);
  void print(FILE *fp, unsigned subpartition_id) const;

  unsigned long long sample_cycles;
  unsigned long long corrected_path_activation_count;
  unsigned long long corrected_path_lowerq_activation_count;
  unsigned long long corrected_path_respq_activation_count;
  unsigned long long corrected_path_dataport_activation_count;
  // Typed proof counters for the directed integrated-pressure fixtures.
  unsigned long long dataport_clean_miss_admit_while_busy;
  unsigned long long dataport_mshr_merge_admit_while_busy;
  unsigned long long dataport_hit_block_cycles;
  unsigned long long missq_merge_admit_while_full;
  unsigned long long missq_clean_miss_admit_one_slot;
  unsigned long long missq_dirty_miss_block_one_slot;
  unsigned long long missq_dirty_miss_admit_two_slots;
  unsigned long long missq_dirty_block_no_mutation;
  unsigned long long missq_dirty_block_partial_mutation;
  unsigned long long dirty_victim_preview_count;
  unsigned long long preview_commit_mismatch;
  unsigned long long mshr_entries_sum, mshr_entries_max;
  unsigned long long mshr_targets_sum, mshr_targets_max;
  unsigned long long mshr_ready_entries_sum, mshr_ready_entries_max;
  unsigned long long mshr_ready_targets_sum, mshr_ready_targets_max;
  unsigned long long missq_sum, missq_max, missq_demand_sum, missq_wb_sum;
  unsigned long long l2_dramq_sum, l2_dramq_max;
  unsigned long long dram_l2q_sum, dram_l2q_max;
  unsigned long long l2_icntq_sum, l2_icntq_max;
  unsigned long long icnt_l2q_sum, icnt_l2q_max;
  unsigned long long rop_sum, rop_max;
  unsigned long long data_port_busy_cycles, fill_port_busy_cycles;
  unsigned long long missq_full_cycles, dram_l2_full_cycles;
  unsigned long long l2_icnt_full_cycles, icnt_l2_full_cycles;
  unsigned long long blocker_cycles[NUM_L2_BLOCK_REASONS];
  unsigned long long blocker_requests[NUM_L2_BLOCK_REASONS];
  unsigned long long blocker_episodes[NUM_L2_BLOCK_REASONS];
};

class partition_mf_allocator : public mem_fetch_allocator {
 public:
  partition_mf_allocator(const memory_config *config) {
    m_memory_config = config;
  }
  virtual mem_fetch *alloc(const class warp_inst_t &inst,
                           const mem_access_t &access,
                           unsigned long long cycle) const {
    abort();
    return NULL;
  }
  virtual mem_fetch *alloc(new_addr_type addr, mem_access_type type,
                           unsigned size, bool wr, unsigned long long cycle,
                           unsigned long long streamID) const;
  virtual mem_fetch *alloc(new_addr_type addr, mem_access_type type,
                           const active_mask_t &active_mask,
                           const mem_access_byte_mask_t &byte_mask,
                           const mem_access_sector_mask_t &sector_mask,
                           unsigned size, bool wr, unsigned long long cycle,
                           unsigned wid, unsigned sid, unsigned tpc,
                           mem_fetch *original_mf,
                           unsigned long long streamID) const;

 private:
  const memory_config *m_memory_config;
};

// Memory partition unit contains all the units assolcated with a single DRAM
// channel.
// - It arbitrates the DRAM channel among multiple sub partitions.
// - It does not connect directly with the interconnection network.
class memory_partition_unit {
 public:
  memory_partition_unit(unsigned partition_id, const memory_config *config,
                        class memory_stats_t *stats, class gpgpu_sim *gpu);
  ~memory_partition_unit();

  bool busy() const;

  void cache_cycle(unsigned cycle);
  void dram_cycle();
  void simple_dram_model_cycle();

  void set_done(mem_fetch *mf);

  void visualizer_print(gzFile visualizer_file) const;
  void print_stat(FILE *fp) { m_dram->print_stat(fp); }
  void visualize() const { m_dram->visualize(); }
  void print(FILE *fp) const;
  void print_ep_l2_b0_snapshot(FILE *fp, unsigned long long uid) const;
  void begin_ep_l2_b0_kernel(unsigned long long uid);
  void end_ep_l2_b0_kernel(FILE *fp, unsigned long long uid);
  void c7e_record_dram_success(bool read, bool write, unsigned bytes);
  void handle_memcpy_to_gpu(size_t dst_start_addr, unsigned subpart_id,
                            mem_access_sector_mask_t mask);

  class memory_sub_partition *get_sub_partition(int sub_partition_id) {
    return m_sub_partition[sub_partition_id];
  }

  // Power model
  void set_dram_power_stats(unsigned &n_cmd, unsigned &n_activity,
                            unsigned &n_nop, unsigned &n_act, unsigned &n_pre,
                            unsigned &n_rd, unsigned &n_wr, unsigned &n_wr_WB,
                            unsigned &n_req) const;

  int global_sub_partition_id_to_local_id(int global_sub_partition_id) const;

  // Directed-test observations of the production L2-to-DRAM arbiter.
  bool l2_char_no_credit_leak() const {
    return m_arbitration_metadata.no_credit_leak();
  }
  unsigned long long l2_char_wb_issued_while_returnq_full() const {
    return m_wb_issued_while_returnq_full;
  }
  unsigned long long l2_char_wb_head_while_returnq_full() const {
    return m_wb_head_while_returnq_full;
  }
  unsigned long long l2_char_read_head_while_returnq_full() const {
    return m_read_head_while_returnq_full;
  }
  unsigned long long l2_char_read_issue_blocked_while_returnq_full() const {
    return m_read_issue_blocked_while_returnq_full;
  }
  // Directed C3b cleanup hook.  It only releases the existing test-only
  // simple-DRAM hold; default production configurations never arm it.
  void l2_char_release_dram_issue_hold() {
    m_l2_char_dram_issue_hold_remaining = 0;
  }

  unsigned get_mpid() const { return m_id; }

  class gpgpu_sim *get_mgpu() const {
    return m_gpu;
  }

 private:
  unsigned m_id;
  const memory_config *m_config;
  class memory_stats_t *m_stats;
  class memory_sub_partition **m_sub_partition;
  class dram_t *m_dram;

  class arbitration_metadata {
   public:
    arbitration_metadata(const memory_config *config);

    // Check general credits and, for no-return traffic, the explicit
    // writeback forward-progress reservation.
    bool has_credits(int inner_sub_partition_id, bool no_return) const;
    // borrow a credit for a subpartition
    void borrow_credit(int inner_sub_partition_id, mem_fetch *mf,
                       bool no_return);
    // return a credit from a subpartition
    void return_credit(int inner_sub_partition_id, mem_fetch *mf);

    // return the last subpartition that borrowed credit
    int last_borrower() const { return m_last_borrower; }

    void print(FILE *fp) const;
    bool no_credit_leak() const;
    unsigned long long wb_progress_credit_use_count() const {
      return m_wb_progress_credit_use_count;
    }
    unsigned wb_progress_credit_current() const {
      return m_wb_progress_credit;
    }
    unsigned wb_progress_credit_max() const { return m_wb_progress_credit_max; }
    unsigned wb_progress_credit_limit() const {
      return m_wb_progress_credit_limit;
    }

   private:
    // id of the last subpartition that borrowed credit
    int m_last_borrower;

    int m_shared_credit_limit;
    int m_private_credit_limit;

    // credits borrowed by the subpartitions
    std::vector<int> m_private_credit;
    int m_shared_credit;
    unsigned m_wb_progress_credit_limit;
    unsigned m_wb_progress_credit;
    unsigned m_wb_progress_credit_max;
    unsigned long long m_wb_progress_credit_use_count;
    std::set<mem_fetch *> m_wb_progress_requests;
  };
  arbitration_metadata m_arbitration_metadata;
  unsigned long long m_wb_issued_while_returnq_full;
  unsigned long long m_wb_head_while_returnq_full;
  unsigned long long m_read_head_while_returnq_full;
  unsigned long long m_read_issue_blocked_while_returnq_full;
  unsigned m_l2_char_dram_issue_hold_remaining;
  unsigned m_l2_char_dram_issue_count;
  unsigned long long m_c7e_dram_cycles, m_c7e_scheduler_occ_sum,
      m_c7e_scheduler_full_cycles, m_c7e_returnq_occ_sum,
      m_c7e_returnq_full_cycles, m_c7e_successful_read_issues,
      m_c7e_successful_write_issues, m_c7e_successful_read_bytes,
      m_c7e_successful_write_bytes;
  unsigned m_c7e_scheduler_occ_max, m_c7e_returnq_occ_max;
  // Per-channel 5K-cycle observation window baselines. These are only
  // snapshots of the application counters above; no scheduling state is
  // derived from them.
  bool m_c7e_window_initialized;
  unsigned long long m_c7e_window_start_cycle, m_c7e_window_dram_cycles,
      m_c7e_window_scheduler_occ_sum, m_c7e_window_scheduler_full_cycles,
      m_c7e_window_returnq_occ_sum, m_c7e_window_returnq_full_cycles,
      m_c7e_window_successful_read_issues,
      m_c7e_window_successful_write_issues,
      m_c7e_window_successful_read_bytes,
      m_c7e_window_successful_write_bytes;
  unsigned m_c7e_window_scheduler_occ_max, m_c7e_window_returnq_occ_max;
  void c7e_sample_dram_cycle();

  bool requires_dram_to_l2_return(const mem_fetch *mf) const;
  // Determine whether this particular request can issue to DRAM.
  bool can_issue_to_dram(int inner_sub_partition_id, const mem_fetch *mf);

  // model DRAM access scheduler latency (fixed latency between L2 and DRAM)
  struct dram_delay_t {
    unsigned long long ready_cycle;
    class mem_fetch *req;
  };
  std::list<dram_delay_t> m_dram_latency_queue;

  class gpgpu_sim *m_gpu;
};

class memory_sub_partition {
 public:
  memory_sub_partition(unsigned sub_partition_id, const memory_config *config,
                       class memory_stats_t *stats, class gpgpu_sim *gpu);
  ~memory_sub_partition();

  unsigned get_id() const { return m_id; }

  bool busy() const;

  void cache_cycle(unsigned cycle);

  bool full() const;
  bool full(unsigned size) const;
  void push(class mem_fetch *mf, unsigned long long clock_cycle);
  class mem_fetch *pop();
  class mem_fetch *top();
  void set_done(mem_fetch *mf);

  unsigned flushL2();
  unsigned invalidateL2();

  // interface to L2_dram_queue
  bool L2_dram_queue_empty() const;
  class mem_fetch *L2_dram_queue_top() const;
  void L2_dram_queue_pop();

  // Observation-only hooks used by memory_partition_unit at real DRAM
  // decision points.  They do not influence arbitration or queue state.
  void l2_char_record_dram_issue(bool is_read, bool is_wb, bool return_block,
                                 bool credit_block, bool scheduler_block,
                                 unsigned scheduler_occupancy);
  void l2_char_record_dram_success(bool is_read, bool is_wb, unsigned bytes);
  void l2_char_record_dram_return(bool eligible, bool blocked);

  // interface to dram_L2_queue
  bool dram_L2_queue_empty() const;
  bool dram_L2_queue_full() const;
  void dram_L2_queue_push(class mem_fetch *mf);
  // Directed-test hook. May only arm after a real response enters ReturnQ.
  void l2_char_hold_returnq(unsigned cycles);

  void visualizer_print(gzFile visualizer_file);
  void print_cache_stat(unsigned &accesses, unsigned &misses) const;
  void print(FILE *fp) const;

  void accumulate_L2cache_stats(class cache_stats &l2_stats) const;
  void get_L2cache_sub_stats(struct cache_sub_stats &css) const;

  // Support for getting per-window L2 stats for AerialVision
  void get_L2cache_sub_stats_pw(struct cache_sub_stats_pw &css) const;
  void clear_L2cache_stats_pw();
  void print_l2_char_stats(FILE *fp) const;
  // C7: a separate target schema. uid is a kernel-launch UID for boundary
  // snapshots; this is statistics-only and never resets cache resources.
  void print_ep_l2_b0_snapshot(FILE *fp, unsigned long long uid) const;
  void begin_ep_l2_b0_kernel(unsigned long long uid);
  void end_ep_l2_b0_kernel(FILE *fp, unsigned long long uid);
  bool l2_char_no_resource_leak() const;

  // EP-L2 C3b observation hooks.  They expose production state for directed
  // regressions and instrumentation only; none participate in admission.
  unsigned ep_l2_descriptor_count_used() const {
    return m_L2cache->ep_l2_descriptor_count_used();
  }
  unsigned ep_l2_line_mshr_entries() const {
    return m_L2cache->mshr_entries_used();
  }
  unsigned ep_l2_ready_requesters() const {
    return m_L2cache->mshr_ready_targets();
  }
  unsigned ep_l2_missq_occupancy() const {
    return m_L2cache->miss_queue_occupancy();
  }
  unsigned ep_l2_wad_occupancy() const { return m_L2cache->ep_l2_wad_occupancy(); }
  unsigned long long ep_l2_wad_full_blocks() const {
    return m_L2cache->ep_l2_wad_full_blocks();
  }
  unsigned long long ep_l2_wad_same_address_waits() const {
    return m_L2cache->ep_l2_wad_same_address_waits();
  }
  unsigned ep_l2_l2dram_occupancy() const {
    return m_L2_dram_queue->get_n_element();
  }
  unsigned ep_l2_draml2_occupancy() const {
    return m_dram_L2_queue->get_n_element();
  }
  unsigned ep_l2_l2icnt_occupancy() const {
    return m_L2_icnt_queue->get_n_element();
  }
  unsigned ep_l2_icntl2_occupancy() const {
    return m_icnt_L2_queue->get_n_element();
  }
  unsigned long long ep_l2_lower_read_issue_count() const {
    return m_ep_l2_lower_read_issue_count;
  }
  unsigned long long ep_l2_payload_identity_lower_issue_count() const {
    return m_ep_l2_payload_identity_lower_issue_count;
  }
  unsigned ep_l2_resident_payload_occupancy() const {
    return m_L2cache->ep_l2_resident_payload_occupancy();
  }
  unsigned ep_l2_resident_payload_pending() const {
    return m_L2cache->ep_l2_resident_pending();
  }
  unsigned ep_l2_resident_payload_valid() const {
    return m_L2cache->ep_l2_resident_valid();
  }
  // M1 directed-test observation: tag metadata must retain only a live
  // static resident handle. This does not participate in cache admission.
  bool ep_l2_tag_payload_consistent() const {
    return m_L2cache->ep_l2_tag_payload_consistent();
  }
  mshr_table::ep_l2_block_reason ep_l2_last_preview_block_reason() const {
    return m_ep_l2_last_preview_block_reason;
  }

  void force_l2_tag_update(new_addr_type addr, unsigned time,
                           mem_access_sector_mask_t mask) {
    m_L2cache->force_tag_access(addr, m_memcpy_cycle_offset + time, mask);
    m_memcpy_cycle_offset += 1;
  }

 private:
  // data
  unsigned m_id;  //< the global sub partition ID
  const memory_config *m_config;
  class l2_cache *m_L2cache;
  class L2interface *m_L2interface;
  class gpgpu_sim *m_gpu;
  partition_mf_allocator *m_mf_allocator;

  // model delay of ROP units with a fixed latency
  struct rop_delay_t {
    unsigned long long ready_cycle;
    class mem_fetch *req;
  };
  std::queue<rop_delay_t> m_rop;

  // these are various FIFOs between units within a memory partition
  fifo_pipeline<mem_fetch> *m_icnt_L2_queue;
  fifo_pipeline<mem_fetch> *m_L2_dram_queue;
  fifo_pipeline<mem_fetch> *m_dram_L2_queue;
  fifo_pipeline<mem_fetch> *m_L2_icnt_queue;  // L2 cache hit response queue
  unsigned m_l2_char_returnq_hold_remaining;

  class mem_fetch *L2dramout;
  unsigned long long int wb_addr;

  class memory_stats_t *m_stats;

  std::set<mem_fetch *> m_request_tracker;
  std::map<mem_fetch *, l2_block_episode_state> m_l2_block_state;
  l2_char_stats m_l2_char_stats;
  l2_char_collector *m_l2_char_collector;
  unsigned m_l2_char_l2dram_class[4];
  unsigned long long m_ep_l2_lower_read_issue_count;
  unsigned long long m_ep_l2_payload_identity_lower_issue_count;
  mshr_table::ep_l2_block_reason m_ep_l2_last_preview_block_reason;

  // EPL2B0V1 is independent of the legacy L2CHARV1 collector. It samples
  // target resources at the real cache-cycle boundary and snapshots those
  // cumulative observations at kernel launch/completion.
  struct ep_l2_b0_accum {
    ep_l2_b0_accum() : samples(0), line_sum(0), desc_sum(0), wad_sum(0),
      resident_sum(0), bypass_sum(0), missq_sum(0), lowerq_sum(0),
      reserved_sum(0), resident_valid_sum(0), resident_dirty_sum(0),
      resident_pending_sum(0), bypass_pending_sum(0), bypass_ready_sum(0),
      descriptor_chain_sum(0), descriptor_chain_samples(0),
      line_max(0), desc_max(0), wad_max(0), resident_max(0), bypass_max(0),
      missq_max(0), lowerq_max(0), reserved_max(0), resident_valid_max(0),
      resident_dirty_max(0), resident_pending_max(0), bypass_pending_max(0),
      bypass_ready_max(0), reserved_set_max(0), descriptor_chain_max(0), descriptor_block(0), wad_block(0), payload_block(0),
      bank_block(0), l1_block(0), lower_block(0), line_alloc_eligible(0),
      line_alloc_block(0), tag_set_all_reserved_block(0),
      line_mshr_alloc_eligible(0), line_mshr_full_block(0),
      descriptor_alloc_eligible(0), descriptor_pool_full_block(0),
      per_address_cap_eligible(0), per_address_cap_block(0), wad_full_events(0),
      wad_hazard_events(0), wad_hazard_wait_cycles(0),
      payload_service_port_denial(0), payload_capacity_allocation_denial(0),
      missq_full_block(0), l2_to_dram_full_block(0), dram_issue_eligible(0),
      dram_read_issues(0), dram_write_issues(0), dram_scheduler_full_block(0),
      dram_returnq_block(0), dram_credit_block(0), dram_return_eligible(0),
      dram_to_l2_full_block(0), c7e_tag_way_alloc_need(0),
      c7e_tag_way_alloc_block(0), c7e_line_mshr_need(0),
      c7e_descriptor_need(0), c7e_per_address_cap_check(0),
      c7e_dram_issue_attempt(0), c7e_dram_successful_read_issues(0),
      c7e_dram_successful_write_issues(0), c7e_dram_successful_read_bytes(0),
      c7e_dram_successful_write_bytes(0), c7e_dram_scheduler_full_observed(0),
      c7e_dram_scheduler_causal_block(0), c7e_dram_to_l2_return_path_block(0),
      dram_scheduler_occ_sum(0),
      dram_scheduler_occ_samples(0), dram_scheduler_occ_max(0) {
      line_hist.assign(1025, 0); desc_hist.assign(257, 0); wad_hist.assign(1025, 0);
      resident_hist.assign(1025, 0); bypass_hist.assign(129, 0);
      reserved_hist.assign(1025, 0); resident_valid_hist.assign(1025, 0);
      resident_dirty_hist.assign(1025, 0); resident_pending_hist.assign(1025, 0);
      bypass_pending_hist.assign(129, 0); bypass_ready_hist.assign(129, 0);
      reserved_set_hist.assign(17, 0); descriptor_chain_hist.assign(33, 0); dram_scheduler_occ_hist.assign(1025, 0);
    }
    unsigned long long samples, line_sum, desc_sum, wad_sum, resident_sum,
        bypass_sum, missq_sum, lowerq_sum, reserved_sum, resident_valid_sum,
        resident_dirty_sum, resident_pending_sum, bypass_pending_sum,
        bypass_ready_sum, descriptor_chain_sum, descriptor_chain_samples;
    unsigned line_max, desc_max, wad_max, resident_max, bypass_max, missq_max,
        lowerq_max, reserved_max, resident_valid_max, resident_dirty_max,
        resident_pending_max, bypass_pending_max, bypass_ready_max, reserved_set_max,
        descriptor_chain_max;
    unsigned long long descriptor_block, wad_block, payload_block, bank_block,
        l1_block, lower_block, line_alloc_eligible, line_alloc_block,
        tag_set_all_reserved_block, line_mshr_alloc_eligible,
        line_mshr_full_block, descriptor_alloc_eligible,
        descriptor_pool_full_block, per_address_cap_eligible,
        per_address_cap_block, wad_full_events, wad_hazard_events,
        wad_hazard_wait_cycles, payload_service_port_denial,
        payload_capacity_allocation_denial, missq_full_block,
        l2_to_dram_full_block, dram_issue_eligible, dram_read_issues,
        dram_write_issues, dram_scheduler_full_block, dram_returnq_block,
        dram_credit_block, dram_return_eligible, dram_to_l2_full_block,
        c7e_tag_way_alloc_need, c7e_tag_way_alloc_block, c7e_line_mshr_need,
        c7e_descriptor_need, c7e_per_address_cap_check, c7e_dram_issue_attempt,
        c7e_dram_successful_read_issues, c7e_dram_successful_write_issues,
        c7e_dram_successful_read_bytes, c7e_dram_successful_write_bytes,
        c7e_dram_scheduler_full_observed, c7e_dram_scheduler_causal_block,
        c7e_dram_to_l2_return_path_block, dram_scheduler_occ_sum,
        dram_scheduler_occ_samples;
    unsigned dram_scheduler_occ_max;
    std::vector<unsigned long long> line_hist, desc_hist, wad_hist,
        resident_hist, bypass_hist, reserved_hist, resident_valid_hist,
        resident_dirty_hist, resident_pending_hist, bypass_pending_hist,
        bypass_ready_hist, reserved_set_hist, descriptor_chain_hist,
        dram_scheduler_occ_hist;
    void sample(unsigned line, unsigned desc, unsigned wad, unsigned resident,
                unsigned bypass, unsigned missq, unsigned lowerq,
                unsigned reserved, unsigned resident_valid,
                unsigned resident_dirty, unsigned resident_pending,
                unsigned bypass_pending, unsigned bypass_ready,
                unsigned reserved_set_max, unsigned descriptor_chain_active,
                unsigned descriptor_chain_sum,
                unsigned descriptor_chain_max,
                const unsigned long long *descriptor_chain_histogram);
    static unsigned p95(const std::vector<unsigned long long> &hist,
                        unsigned long long n);
    ep_l2_b0_accum delta(const ep_l2_b0_accum &start) const;
  };
  ep_l2_b0_accum m_ep_l2_b0_accum;
  std::map<unsigned long long, ep_l2_b0_accum> m_ep_l2_b0_kernel_start;
  struct ep_l2_b0_bank_accum {
    ep_l2_b0_bank_accum() : requests(0), grants(0), conflicts(0), logical(0),
      attempts(0), retries(0), true_ops(0), true_events(0), wait(0),
      resident_hit_read(0), resident_write(0), fill_write(0), wb_readout(0),
      bypass_fill(0), bypass_read(0) {
      for (unsigned b = 0; b < 4; ++b)
        logical_by_bank[b] = grants_by_bank[b] = true_ops_by_bank[b] =
            true_events_by_bank[b] = wait_by_bank[b] = 0;
    }
    unsigned long long requests, grants, conflicts, logical, attempts, retries,
        true_ops, true_events, wait, logical_by_bank[4], grants_by_bank[4],
        true_ops_by_bank[4], true_events_by_bank[4], wait_by_bank[4],
        resident_hit_read, resident_write, fill_write, wb_readout, bypass_fill,
        bypass_read;
    ep_l2_b0_bank_accum delta(const ep_l2_b0_bank_accum &start) const;
  };
  ep_l2_b0_bank_accum ep_l2_b0_bank_snapshot() const;
  std::map<unsigned long long, ep_l2_b0_bank_accum> m_ep_l2_b0_kernel_bank_start;
  // Small C7d temporal view: one retained baseline per physical slice and
  // one emitted record per 5K simulated cycles.  It is deliberately not the
  // legacy L2CHAR collector.
  ep_l2_b0_accum m_ep_l2_b0_window_start;
  ep_l2_b0_bank_accum m_ep_l2_b0_window_bank_start;
  unsigned long long m_ep_l2_b0_window_start_cycle;
  bool m_ep_l2_b0_window_started;
  std::map<unsigned long long, unsigned long long> m_ep_l2_b0_kernel_start_cycle;
  std::map<unsigned long long, bool> m_ep_l2_b0_kernel_overlap;

  // M0a is intentionally a compact, separate schema from the reviewed B0
  // calibration stream.  All fields are sampled/counted after their actual
  // production predicate or commit point; no field is read by control logic.
  struct ep_l2_m0a_accum {
    ep_l2_m0a_accum()
        : resident_samples(0), resident_occupied_sum(0), resident_free_sum(0),
          resident_occupied_max(0), resident_free_min(1024), observed(0),
          any_blocked(0), tag_way(0), wad_full(0), wad_hazard(0),
          line_mshr(0), descriptor(0), per_address(0), missq(0),
          payload_service(0), payload_capacity(0), lowerq(0), responseq(0),
          useful_frontend_admit(0), useful_response_enqueue(0) {}
    unsigned long long resident_samples, resident_occupied_sum, resident_free_sum;
    unsigned resident_occupied_max, resident_free_min;
    unsigned long long observed, any_blocked, tag_way, wad_full, wad_hazard,
        line_mshr, descriptor, per_address, missq, payload_service,
        payload_capacity, lowerq, responseq, useful_frontend_admit,
        useful_response_enqueue;
    ep_l2_m0a_accum delta(const ep_l2_m0a_accum &start) const;
    void sample_resident(unsigned occupied, unsigned capacity);
  };
  ep_l2_m0a_accum m_ep_l2_m0a_accum;
  std::map<unsigned long long, ep_l2_m0a_accum> m_ep_l2_m0a_kernel_start;
  ep_l2_m0a_accum m_ep_l2_m0a_window_start;
  unsigned long long m_ep_l2_m0a_window_start_cycle;
  bool m_ep_l2_m0a_window_started;

  void l2_char_record_l2dram_push(mem_fetch *mf);
  void ep_l2_record_lower_issue(mem_fetch *mf);
  void l2_char_record_l2dram_pop(mem_fetch *mf);
  void l2_char_sample(unsigned long long cycle);
  void ep_l2_b0_sample(unsigned long long cycle);
  void ep_l2_m0a_sample(unsigned long long cycle);
  void ep_l2_m0a_record_frontend(const l2_access_plan &plan,
                                 const l2_admission_inputs &admission,
                                 bool admit);
  void ep_l2_m0a_print(FILE *fp, const ep_l2_m0a_accum &stats,
                       const char *scope, const char *interval,
                       unsigned long long uid, unsigned long long start_cycle,
                       unsigned long long completion_cycle) const;
  void ep_l2_m0a_record_response_enqueue();
  // EPL2MOTV1 is intentionally separate from M0a: these routines only
  // observe the already-decided frontend/dirty-WB stream.
  struct ep_l2_motivation_stats {
    ep_l2_motivation_stats();
    unsigned long long eligible_refs, excluded_wb_refs, reuse_instances;
    unsigned long long reuse_bins[9];
    unsigned long long unique_lines, unique_reused, one_touch_lines;
    unsigned long long post_evictions, post_eviction_rerefs;
    unsigned long long post_eviction_seq_sum, post_eviction_cycle_sum;
    unsigned long long eligible_miss_cycles[3], blocked_cycles[3];
    unsigned long long blocks[3][5];
    unsigned long long wbuf_opportunities[3], wbuf_would_block[3];
    unsigned long long wb_created, wb_lower_accepted, wb_lifetime_sum,
        wb_lifetime_max;
  };
  struct ep_l2_motivation_eviction {
    unsigned long long sequence, cycle;
    ep_l2_motivation_eviction() : sequence(0), cycle(0) {}
    ep_l2_motivation_eviction(unsigned long long s, unsigned long long c)
        : sequence(s), cycle(c) {}
  };
  ep_l2_motivation_stats m_ep_l2_motivation_total;
  ep_l2_motivation_stats m_ep_l2_motivation_epoch;
  std::map<new_addr_type, unsigned> m_ep_l2_motivation_touches;
  std::list<new_addr_type> m_ep_l2_motivation_stack;
  std::map<new_addr_type, std::list<new_addr_type>::iterator>
      m_ep_l2_motivation_stack_pos;
  std::map<new_addr_type, ep_l2_motivation_eviction>
      m_ep_l2_motivation_evictions;
  std::map<new_addr_type, unsigned long long> m_ep_l2_motivation_active_wb;
  std::set<mem_fetch *> m_ep_l2_motivation_seen_frontend;
  unsigned long long m_ep_l2_motivation_sequence;
  void ep_l2_motivation_reset_epoch();
  void ep_l2_motivation_record_reference(mem_fetch *mf,
                                         unsigned long long cycle);
  void ep_l2_motivation_record_frontend(const l2_access_plan &plan,
                                        const l2_admission_inputs &admission,
                                        bool admitted);
  void ep_l2_motivation_record_wb_create(new_addr_type block,
                                         unsigned long long cycle);
  void ep_l2_motivation_record_eviction(new_addr_type block,
                                        unsigned long long cycle);
  void ep_l2_motivation_record_wb_lower_accept(mem_fetch *mf,
                                               unsigned long long cycle);
  void ep_l2_motivation_print(FILE *fp, const char *scope,
                              unsigned long long kernel_uid) const;
  static unsigned l2_char_queue_class(const mem_fetch *mf);

  friend class L2interface;

  std::vector<mem_fetch *> breakdown_request_to_sector_requests(mem_fetch *mf);

  // This is a cycle offset that has to be applied to the l2 accesses to account
  // for the cudamemcpy read/writes. We want GPGPU-Sim to only count cycles for
  // kernel execution but we want cudamemcpy to go through the L2. Everytime an
  // access is made from cudamemcpy this counter is incremented, and when the l2
  // is accessed (in both cudamemcpyies and otherwise) this value is added to
  // the gpgpu-sim cycle counters.
  unsigned m_memcpy_cycle_offset;
};

class L2interface : public mem_fetch_interface {
 public:
  L2interface(memory_sub_partition *unit) { m_unit = unit; }
  virtual ~L2interface() {}
  virtual bool full(unsigned size, bool write) const {
    // assume read and write packets all same size
    return m_unit->m_L2_dram_queue->full();
  }
  virtual void push(mem_fetch *mf) {
    mf->set_status(IN_PARTITION_L2_TO_DRAM_QUEUE, 0 /*FIXME*/);
    m_unit->m_L2_dram_queue->push(mf);
    m_unit->l2_char_record_l2dram_push(mf);
    m_unit->ep_l2_record_lower_issue(mf);
  }

 private:
  memory_sub_partition *m_unit;
};

#endif
