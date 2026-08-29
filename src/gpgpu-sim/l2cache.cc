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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <set>

#include "../abstract_hardware_model.h"
#include "../option_parser.h"
#include "../statwrapper.h"
#include "dram.h"
#include "gpu-cache.h"
#include "gpu-sim.h"
#include "histogram.h"
#include "l2cache.h"
#include "l2cache_trace.h"
#include "mem_fetch.h"
#include "mem_latency_stat.h"
#include "shader.h"

namespace {
const char *const k_l2_block_reason_name[NUM_L2_BLOCK_REASONS] = {
    "line_alloc", "mshr_new", "mshr_merge", "missq",
    "data_port",  "respq",    "other"};

void l2_char_update_max(unsigned long long value, unsigned long long &maximum) {
  if (value > maximum) maximum = value;
}
}  // namespace

void l2_char_stats::clear() {
  sample_cycles = 0;
  corrected_path_activation_count = 0;
  corrected_path_lowerq_activation_count = 0;
  corrected_path_respq_activation_count = 0;
  corrected_path_dataport_activation_count = 0;
  dataport_clean_miss_admit_while_busy = 0;
  dataport_mshr_merge_admit_while_busy = 0;
  dataport_hit_block_cycles = 0;
  missq_merge_admit_while_full = 0;
  missq_clean_miss_admit_one_slot = 0;
  missq_dirty_miss_block_one_slot = 0;
  missq_dirty_miss_admit_two_slots = 0;
  missq_dirty_block_no_mutation = 0;
  missq_dirty_block_partial_mutation = 0;
  dirty_victim_preview_count = 0;
  preview_commit_mismatch = 0;
  mshr_entries_sum = mshr_entries_max = 0;
  mshr_targets_sum = mshr_targets_max = 0;
  mshr_ready_entries_sum = mshr_ready_entries_max = 0;
  mshr_ready_targets_sum = mshr_ready_targets_max = 0;
  missq_sum = missq_max = missq_demand_sum = missq_wb_sum = 0;
  l2_dramq_sum = l2_dramq_max = 0;
  dram_l2q_sum = dram_l2q_max = 0;
  l2_icntq_sum = l2_icntq_max = 0;
  icnt_l2q_sum = icnt_l2q_max = 0;
  rop_sum = rop_max = 0;
  data_port_busy_cycles = fill_port_busy_cycles = 0;
  missq_full_cycles = dram_l2_full_cycles = 0;
  l2_icnt_full_cycles = icnt_l2_full_cycles = 0;
  for (unsigned i = 0; i < NUM_L2_BLOCK_REASONS; ++i) {
    blocker_cycles[i] = 0;
    blocker_requests[i] = 0;
    blocker_episodes[i] = 0;
  }
}

void l2_char_stats::sample(
    unsigned mshr_entries, unsigned mshr_targets, unsigned mshr_ready_entries,
    unsigned mshr_ready_targets, unsigned missq, unsigned missq_demand,
    unsigned missq_wb, unsigned l2_dramq, unsigned dram_l2q,
    unsigned l2_icntq, unsigned icnt_l2q, unsigned rop, bool data_busy,
    bool fill_busy, bool missq_full, bool dram_l2_full, bool l2_icnt_full,
    bool icnt_l2_full) {
  sample_cycles++;
  mshr_entries_sum += mshr_entries;
  mshr_targets_sum += mshr_targets;
  mshr_ready_entries_sum += mshr_ready_entries;
  mshr_ready_targets_sum += mshr_ready_targets;
  missq_sum += missq;
  missq_demand_sum += missq_demand;
  missq_wb_sum += missq_wb;
  l2_dramq_sum += l2_dramq;
  dram_l2q_sum += dram_l2q;
  l2_icntq_sum += l2_icntq;
  icnt_l2q_sum += icnt_l2q;
  rop_sum += rop;
  l2_char_update_max(mshr_entries, mshr_entries_max);
  l2_char_update_max(mshr_targets, mshr_targets_max);
  l2_char_update_max(mshr_ready_entries, mshr_ready_entries_max);
  l2_char_update_max(mshr_ready_targets, mshr_ready_targets_max);
  l2_char_update_max(missq, missq_max);
  l2_char_update_max(l2_dramq, l2_dramq_max);
  l2_char_update_max(dram_l2q, dram_l2q_max);
  l2_char_update_max(l2_icntq, l2_icntq_max);
  l2_char_update_max(icnt_l2q, icnt_l2q_max);
  l2_char_update_max(rop, rop_max);
  if (data_busy) data_port_busy_cycles++;
  if (fill_busy) fill_port_busy_cycles++;
  if (missq_full) missq_full_cycles++;
  if (dram_l2_full) dram_l2_full_cycles++;
  if (l2_icnt_full) l2_icnt_full_cycles++;
  if (icnt_l2_full) icnt_l2_full_cycles++;
}

void l2_char_stats::record_blockers(
    mem_fetch *mf, unsigned long long blocker_mask, unsigned long long cycle,
    std::map<mem_fetch *, l2_block_episode_state> &state) {
  if (!blocker_mask) return;
  l2_block_episode_state &entry = state[mf];
  if (!entry.first_block_cycle) entry.first_block_cycle = cycle;
  entry.total_block_cycles++;
  for (unsigned reason = 0; reason < NUM_L2_BLOCK_REASONS; ++reason) {
    unsigned long long bit = 1ULL << reason;
    if (!(blocker_mask & bit)) continue;
    blocker_cycles[reason]++;
    if (!(entry.ever_blocked_mask & bit)) {
      blocker_requests[reason]++;
      entry.ever_blocked_mask |= bit;
    }
    if (!(entry.prev_blocked_mask & bit)) blocker_episodes[reason]++;
  }
  entry.prev_blocked_mask = blocker_mask;
}

void l2_char_stats::print(FILE *fp, unsigned subpartition_id) const {
  double denom = sample_cycles ? sample_cycles : 1;
  fprintf(fp, "L2_char_subpartition = %u\n", subpartition_id);
  fprintf(fp, "L2_char_corrected_path_activation_count = %llu\n",
          corrected_path_activation_count);
  fprintf(fp, "L2_char_corrected_path_lowerq_activation_count = %llu\n",
          corrected_path_lowerq_activation_count);
  fprintf(fp, "L2_char_corrected_path_respq_activation_count = %llu\n",
          corrected_path_respq_activation_count);
  fprintf(fp, "L2_char_corrected_path_dataport_activation_count = %llu\n",
          corrected_path_dataport_activation_count);
  fprintf(fp, "L2_char_dataport_clean_miss_admit_while_busy = %llu\n",
          dataport_clean_miss_admit_while_busy);
  fprintf(fp, "L2_char_dataport_mshr_merge_admit_while_busy = %llu\n",
          dataport_mshr_merge_admit_while_busy);
  fprintf(fp, "L2_char_dataport_hit_block_cycles = %llu\n",
          dataport_hit_block_cycles);
  fprintf(fp, "L2_char_missq_merge_admit_while_full = %llu\n",
          missq_merge_admit_while_full);
  fprintf(fp, "L2_char_missq_clean_miss_admit_one_slot = %llu\n",
          missq_clean_miss_admit_one_slot);
  fprintf(fp, "L2_char_missq_dirty_miss_block_one_slot = %llu\n",
          missq_dirty_miss_block_one_slot);
  fprintf(fp, "L2_char_missq_dirty_miss_admit_two_slots = %llu\n",
          missq_dirty_miss_admit_two_slots);
  fprintf(fp, "L2_char_missq_dirty_block_no_mutation = %llu\n",
          missq_dirty_block_no_mutation);
  fprintf(fp, "L2_char_missq_dirty_block_partial_mutation = %llu\n",
          missq_dirty_block_partial_mutation);
  fprintf(fp, "L2_char_dirty_victim_preview_count = %llu\n",
          dirty_victim_preview_count);
  fprintf(fp, "L2_char_preview_commit_mismatch = %llu\n",
          preview_commit_mismatch);
  fprintf(fp, "L2_char_mshr_entries_avg = %.6f\n",
          mshr_entries_sum / denom);
  fprintf(fp, "L2_char_mshr_entries_max = %llu\n", mshr_entries_max);
  fprintf(fp, "L2_char_mshr_targets_avg = %.6f\n",
          mshr_targets_sum / denom);
  fprintf(fp, "L2_char_mshr_targets_max = %llu\n", mshr_targets_max);
  fprintf(fp, "L2_char_mshr_ready_entries_avg = %.6f\n",
          mshr_ready_entries_sum / denom);
  fprintf(fp, "L2_char_mshr_ready_targets_avg = %.6f\n",
          mshr_ready_targets_sum / denom);
  fprintf(fp, "L2_char_missq_avg = %.6f\n", missq_sum / denom);
  fprintf(fp, "L2_char_missq_max = %llu\n", missq_max);
  fprintf(fp, "L2_char_missq_demand_avg = %.6f\n", missq_demand_sum / denom);
  fprintf(fp, "L2_char_missq_wb_avg = %.6f\n", missq_wb_sum / denom);
  fprintf(fp, "L2_char_l2_dramq_avg = %.6f\n", l2_dramq_sum / denom);
  fprintf(fp, "L2_char_dram_l2q_avg = %.6f\n", dram_l2q_sum / denom);
  fprintf(fp, "L2_char_l2_icntq_avg = %.6f\n", l2_icntq_sum / denom);
  fprintf(fp, "L2_char_icnt_l2q_avg = %.6f\n", icnt_l2q_sum / denom);
  fprintf(fp, "L2_char_rop_avg = %.6f\n", rop_sum / denom);
  fprintf(fp, "L2_char_data_port_busy_cycles = %llu\n",
          data_port_busy_cycles);
  fprintf(fp, "L2_char_fill_port_busy_cycles = %llu\n",
          fill_port_busy_cycles);
  fprintf(fp, "L2_char_missq_full_cycles = %llu\n", missq_full_cycles);
  fprintf(fp, "L2_char_dram_l2q_full_cycles = %llu\n",
          dram_l2_full_cycles);
  fprintf(fp, "L2_char_l2_icntq_full_cycles = %llu\n",
          l2_icnt_full_cycles);
  fprintf(fp, "L2_char_icnt_l2q_full_cycles = %llu\n",
          icnt_l2_full_cycles);
  for (unsigned reason = 0; reason < NUM_L2_BLOCK_REASONS; ++reason) {
    fprintf(fp, "L2_char_block_cycles_%s = %llu\n",
            k_l2_block_reason_name[reason], blocker_cycles[reason]);
    fprintf(fp, "L2_char_block_requests_%s = %llu\n",
            k_l2_block_reason_name[reason], blocker_requests[reason]);
    fprintf(fp, "L2_char_block_episodes_%s = %llu\n",
            k_l2_block_reason_name[reason], blocker_episodes[reason]);
  }
}

mem_fetch *partition_mf_allocator::alloc(new_addr_type addr,
                                         mem_access_type type, unsigned size,
                                         bool wr, unsigned long long cycle,
                                         unsigned long long streamID) const {
  assert(wr);
  mem_access_t access(type, addr, size, wr, m_memory_config->gpgpu_ctx);
  mem_fetch *mf = new mem_fetch(access, NULL, streamID, WRITE_PACKET_SIZE, -1,
                                -1, -1, m_memory_config, cycle);
  return mf;
}

mem_fetch *partition_mf_allocator::alloc(
    new_addr_type addr, mem_access_type type, const active_mask_t &active_mask,
    const mem_access_byte_mask_t &byte_mask,
    const mem_access_sector_mask_t &sector_mask, unsigned size, bool wr,
    unsigned long long cycle, unsigned wid, unsigned sid, unsigned tpc,
    mem_fetch *original_mf, unsigned long long streamID) const {
  mem_access_t access(type, addr, size, wr, active_mask, byte_mask, sector_mask,
                      m_memory_config->gpgpu_ctx);
  mem_fetch *mf = new mem_fetch(access, NULL, streamID,
                                wr ? WRITE_PACKET_SIZE : READ_PACKET_SIZE, wid,
                                sid, tpc, m_memory_config, cycle, original_mf);
  return mf;
}
memory_partition_unit::memory_partition_unit(unsigned partition_id,
                                             const memory_config *config,
                                             class memory_stats_t *stats,
                                             class gpgpu_sim *gpu)
    : m_id(partition_id),
      m_config(config),
      m_stats(stats),
      m_arbitration_metadata(config),
      m_wb_issued_while_returnq_full(0),
      m_wb_head_while_returnq_full(0),
      m_read_head_while_returnq_full(0),
      m_read_issue_blocked_while_returnq_full(0),
      m_l2_char_dram_issue_hold_remaining(
          config->gpgpu_l2_char_dram_issue_hold_cycles),
      m_l2_char_dram_issue_count(0),
      m_c7e_dram_cycles(0),
      m_c7e_scheduler_occ_sum(0),
      m_c7e_scheduler_full_cycles(0),
      m_c7e_returnq_occ_sum(0),
      m_c7e_returnq_full_cycles(0),
      m_c7e_successful_read_issues(0),
      m_c7e_successful_write_issues(0),
      m_c7e_successful_read_bytes(0),
      m_c7e_successful_write_bytes(0),
      m_c7e_scheduler_occ_max(0),
      m_c7e_returnq_occ_max(0),
      m_c7e_window_initialized(false),
      m_c7e_window_start_cycle(0),
      m_c7e_window_dram_cycles(0),
      m_c7e_window_scheduler_occ_sum(0),
      m_c7e_window_scheduler_full_cycles(0),
      m_c7e_window_returnq_occ_sum(0),
      m_c7e_window_returnq_full_cycles(0),
      m_c7e_window_successful_read_issues(0),
      m_c7e_window_successful_write_issues(0),
      m_c7e_window_successful_read_bytes(0),
      m_c7e_window_successful_write_bytes(0),
      m_c7e_window_scheduler_occ_max(0),
      m_c7e_window_returnq_occ_max(0),
      m_gpu(gpu) {
  m_dram = new dram_t(m_id, m_config, m_stats, this, gpu);

  m_sub_partition = new memory_sub_partition
      *[m_config->m_n_sub_partition_per_memory_channel];
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    unsigned sub_partition_id =
        m_id * m_config->m_n_sub_partition_per_memory_channel + p;
    m_sub_partition[p] =
        new memory_sub_partition(sub_partition_id, m_config, stats, gpu);
  }
}

void memory_partition_unit::c7e_sample_dram_cycle() {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  const unsigned scheduler = m_dram->que_length();
  const unsigned returnq = m_dram->returnq_length();
  ++m_c7e_dram_cycles;
  m_c7e_scheduler_occ_sum += scheduler;
  m_c7e_returnq_occ_sum += returnq;
  m_c7e_scheduler_occ_max = std::max(m_c7e_scheduler_occ_max, scheduler);
  m_c7e_returnq_occ_max = std::max(m_c7e_returnq_occ_max, returnq);
  if (m_dram->full(false)) ++m_c7e_scheduler_full_cycles;
  if (m_dram->returnq_full()) ++m_c7e_returnq_full_cycles;

  const unsigned long long cycle =
      m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle;
  if (!m_c7e_window_initialized) {
    m_c7e_window_initialized = true;
    m_c7e_window_start_cycle = cycle;
    m_c7e_window_dram_cycles = m_c7e_dram_cycles;
    m_c7e_window_scheduler_occ_sum = m_c7e_scheduler_occ_sum;
    m_c7e_window_scheduler_full_cycles = m_c7e_scheduler_full_cycles;
    m_c7e_window_returnq_occ_sum = m_c7e_returnq_occ_sum;
    m_c7e_window_returnq_full_cycles = m_c7e_returnq_full_cycles;
    m_c7e_window_successful_read_issues = m_c7e_successful_read_issues;
    m_c7e_window_successful_write_issues = m_c7e_successful_write_issues;
    m_c7e_window_successful_read_bytes = m_c7e_successful_read_bytes;
    m_c7e_window_successful_write_bytes = m_c7e_successful_write_bytes;
    m_c7e_window_scheduler_occ_max = scheduler;
    m_c7e_window_returnq_occ_max = returnq;
    return;
  }

  if (cycle - m_c7e_window_start_cycle < 5000) {
    m_c7e_window_scheduler_occ_max =
        std::max(m_c7e_window_scheduler_occ_max, scheduler);
    m_c7e_window_returnq_occ_max =
        std::max(m_c7e_window_returnq_occ_max, returnq);
    return;
  }

  const unsigned long long window_cycles =
      m_c7e_dram_cycles - m_c7e_window_dram_cycles;
  const unsigned long long scheduler_sum =
      m_c7e_scheduler_occ_sum - m_c7e_window_scheduler_occ_sum;
  const unsigned long long returnq_sum =
      m_c7e_returnq_occ_sum - m_c7e_window_returnq_occ_sum;
  const unsigned long long scheduler_full =
      m_c7e_scheduler_full_cycles - m_c7e_window_scheduler_full_cycles;
  const unsigned long long returnq_full =
      m_c7e_returnq_full_cycles - m_c7e_window_returnq_full_cycles;
  const unsigned long long read_issues =
      m_c7e_successful_read_issues - m_c7e_window_successful_read_issues;
  const unsigned long long write_issues =
      m_c7e_successful_write_issues - m_c7e_window_successful_write_issues;
  const unsigned long long read_bytes =
      m_c7e_successful_read_bytes - m_c7e_window_successful_read_bytes;
  const unsigned long long write_bytes =
      m_c7e_successful_write_bytes - m_c7e_window_successful_write_bytes;
  const unsigned long long bytes = read_bytes + write_bytes;
  const unsigned long long capacity =
      window_cycles * (unsigned long long)m_config->dram_atom_size;
  printf(
      "EPL2DRAMV1|scope=window|interval=5000_cycle|channel=%u|"
      "window_start_cycle=%llu|window_end_cycle=%llu|dram_cycles=%llu|"
      "scheduler_occ_avg=%llu|scheduler_occ_max=%u|scheduler_full_cycles=%llu|"
      "returnq_occ_avg=%llu|returnq_occ_max=%u|returnq_full_cycles=%llu|"
      "successful_read_issues=%llu|successful_write_issues=%llu|"
      "successful_read_bytes=%llu|successful_write_bytes=%llu|"
      "bandwidth_util_numerator_bytes=%llu|bandwidth_util_denominator_bytes=%llu|"
      "bandwidth_util=%0.9f\n",
      m_id, m_c7e_window_start_cycle, cycle, window_cycles,
      window_cycles ? scheduler_sum / window_cycles : 0,
      m_c7e_window_scheduler_occ_max, scheduler_full,
      window_cycles ? returnq_sum / window_cycles : 0,
      m_c7e_window_returnq_occ_max, returnq_full, read_issues, write_issues,
      read_bytes, write_bytes, bytes, capacity,
      capacity ? (double)bytes / capacity : 0.0);

  m_c7e_window_start_cycle = cycle;
  m_c7e_window_dram_cycles = m_c7e_dram_cycles;
  m_c7e_window_scheduler_occ_sum = m_c7e_scheduler_occ_sum;
  m_c7e_window_scheduler_full_cycles = m_c7e_scheduler_full_cycles;
  m_c7e_window_returnq_occ_sum = m_c7e_returnq_occ_sum;
  m_c7e_window_returnq_full_cycles = m_c7e_returnq_full_cycles;
  m_c7e_window_successful_read_issues = m_c7e_successful_read_issues;
  m_c7e_window_successful_write_issues = m_c7e_successful_write_issues;
  m_c7e_window_successful_read_bytes = m_c7e_successful_read_bytes;
  m_c7e_window_successful_write_bytes = m_c7e_successful_write_bytes;
  m_c7e_window_scheduler_occ_max = scheduler;
  m_c7e_window_returnq_occ_max = returnq;
}

void memory_partition_unit::c7e_record_dram_success(bool read, bool write,
                                                      unsigned bytes) {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  if (read) {
    ++m_c7e_successful_read_issues;
    m_c7e_successful_read_bytes += bytes;
  }
  if (write) {
    ++m_c7e_successful_write_issues;
    m_c7e_successful_write_bytes += bytes;
  }
}

void memory_partition_unit::handle_memcpy_to_gpu(
    size_t addr, unsigned global_subpart_id, mem_access_sector_mask_t mask) {
  unsigned p = global_sub_partition_id_to_local_id(global_subpart_id);
  std::string mystring = mask.to_string<char, std::string::traits_type,
                                        std::string::allocator_type>();
  MEMPART_DPRINTF(
      "Copy Engine Request Received For Address=%zx, local_subpart=%u, "
      "global_subpart=%u, sector_mask=%s \n",
      addr, p, global_subpart_id, mystring.c_str());
  m_sub_partition[p]->force_l2_tag_update(
      addr, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle, mask);
}

memory_partition_unit::~memory_partition_unit() {
  delete m_dram;
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    delete m_sub_partition[p];
  }
  delete[] m_sub_partition;
}

memory_partition_unit::arbitration_metadata::arbitration_metadata(
    const memory_config *config)
    : m_last_borrower(config->m_n_sub_partition_per_memory_channel - 1),
      m_private_credit(config->m_n_sub_partition_per_memory_channel, 0),
      m_shared_credit(0),
      m_wb_progress_credit_limit(config->gpgpu_l2_wb_progress_credit),
      m_wb_progress_credit(0),
      m_wb_progress_credit_max(0),
      m_wb_progress_credit_use_count(0) {
  // each sub partition get at least 1 credit for forward progress
  // the rest is shared among with other partitions
  m_private_credit_limit = 1;
  m_shared_credit_limit = config->gpgpu_frfcfs_dram_sched_queue_size +
                          config->gpgpu_dram_return_queue_size -
                          (config->m_n_sub_partition_per_memory_channel - 1);
  if (config->seperate_write_queue_enabled)
    m_shared_credit_limit += config->gpgpu_frfcfs_dram_write_queue_size;
  if (config->gpgpu_frfcfs_dram_sched_queue_size == 0 or
      config->gpgpu_dram_return_queue_size == 0) {
    m_shared_credit_limit =
        0;  // no limit if either of the queue has no limit in size
  }
  assert(m_shared_credit_limit >= 0);
}

bool memory_partition_unit::arbitration_metadata::has_credits(
    int inner_sub_partition_id, bool no_return) const {
  int spid = inner_sub_partition_id;
  if (m_private_credit[spid] < m_private_credit_limit) {
    return true;
  } else if (m_shared_credit_limit == 0 ||
             m_shared_credit < m_shared_credit_limit) {
    return true;
  } else if (no_return &&
             m_wb_progress_credit < m_wb_progress_credit_limit) {
    return true;
  } else {
    return false;
  }
}

void memory_partition_unit::arbitration_metadata::borrow_credit(
    int inner_sub_partition_id, mem_fetch *mf, bool no_return) {
  int spid = inner_sub_partition_id;
  if (m_private_credit[spid] < m_private_credit_limit) {
    m_private_credit[spid] += 1;
  } else if (m_shared_credit_limit == 0 ||
             m_shared_credit < m_shared_credit_limit) {
    m_shared_credit += 1;
  } else if (no_return &&
             m_wb_progress_credit < m_wb_progress_credit_limit) {
    m_wb_progress_credit++;
    m_wb_progress_credit_max =
        std::max(m_wb_progress_credit_max, m_wb_progress_credit);
    m_wb_progress_credit_use_count++;
    bool inserted = m_wb_progress_requests.insert(mf).second;
    assert(inserted);
  } else {
    assert(0 && "DRAM arbitration error: Borrowing from depleted credit!");
  }
  m_last_borrower = spid;
}

void memory_partition_unit::arbitration_metadata::return_credit(
    int inner_sub_partition_id, mem_fetch *mf) {
  int spid = inner_sub_partition_id;
  if (m_wb_progress_requests.erase(mf)) {
    assert(m_wb_progress_credit > 0);
    m_wb_progress_credit--;
    return;
  }
  if (m_private_credit[spid] > 0) {
    m_private_credit[spid] -= 1;
  } else {
    m_shared_credit -= 1;
  }
  assert((m_shared_credit >= 0) &&
         "DRAM arbitration error: Returning more than available credits!");
}

void memory_partition_unit::arbitration_metadata::print(FILE *fp) const {
  fprintf(fp, "private_credit = ");
  for (unsigned p = 0; p < m_private_credit.size(); p++) {
    fprintf(fp, "%d ", m_private_credit[p]);
  }
  fprintf(fp, "(limit = %d)\n", m_private_credit_limit);
  fprintf(fp, "shared_credit = %d (limit = %d)\n", m_shared_credit,
          m_shared_credit_limit);
  fprintf(fp, "wb_progress_credit = %u (limit = %u)\n",
          m_wb_progress_credit, m_wb_progress_credit_limit);
  fprintf(fp, "L2_char_wb_progress_credit_max = %u\n",
          m_wb_progress_credit_max);
  fprintf(fp, "L2_char_wb_progress_credit_use_count = %llu\n",
          m_wb_progress_credit_use_count);
}

bool memory_partition_unit::arbitration_metadata::no_credit_leak() const {
  if (m_shared_credit != 0 || m_wb_progress_credit != 0 ||
      !m_wb_progress_requests.empty())
    return false;
  for (unsigned p = 0; p < m_private_credit.size(); ++p) {
    if (m_private_credit[p] != 0) return false;
  }
  return true;
}

bool memory_partition_unit::busy() const {
  // A request can leave its subpartition queue while still traversing the
  // partition's DRAM-latency/DRAM path.  Its borrowed credit is the durable
  // ownership record for that interval (in particular for no-return L2
  // writebacks), so termination must keep cycling until it is returned.
  if (!m_dram_latency_queue.empty() || !m_arbitration_metadata.no_credit_leak())
    return true;

  bool busy = false;
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    if (m_sub_partition[p]->busy()) {
      busy = true;
    }
  }
  return busy;
}

void memory_partition_unit::cache_cycle(unsigned cycle) {
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    m_sub_partition[p]->cache_cycle(cycle);
  }
}

void memory_partition_unit::visualizer_print(gzFile visualizer_file) const {
  m_dram->visualizer_print(visualizer_file);
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    m_sub_partition[p]->visualizer_print(visualizer_file);
  }
}

bool memory_partition_unit::requires_dram_to_l2_return(
    const mem_fetch *mf) const {
  return mf->get_access_type() != L1_WRBK_ACC &&
         mf->get_access_type() != L2_WRBK_ACC;
}

// Determine whether this particular request can issue to DRAM.  Writebacks
// complete at DRAM and therefore do not consume the destination return FIFO.
bool memory_partition_unit::can_issue_to_dram(int inner_sub_partition_id,
                                               const mem_fetch *mf) {
  int spid = inner_sub_partition_id;
  bool needs_return = requires_dram_to_l2_return(mf);
  bool returnq_full = m_sub_partition[spid]->dram_L2_queue_full();
  bool return_path_full = needs_return && returnq_full;
  bool has_dram_resource =
      m_arbitration_metadata.has_credits(spid, !needs_return);
  bool has_general_credit = m_arbitration_metadata.has_credits(spid, false);

  if (returnq_full && needs_return) {
    m_read_head_while_returnq_full++;
    if (has_general_credit) m_read_issue_blocked_while_returnq_full++;
  } else if (returnq_full) {
    m_wb_head_while_returnq_full++;
  }

  MEMPART_DPRINTF(
      "sub partition %d return_path_full=%c has_dram_resource=%c\n", spid,
      return_path_full ? 'T' : 'F',
      (has_dram_resource) ? 'T' : 'F');

  return dram_issue_allowed(needs_return, return_path_full,
                            has_general_credit,
                            !needs_return && has_dram_resource);
}

int memory_partition_unit::global_sub_partition_id_to_local_id(
    int global_sub_partition_id) const {
  return (global_sub_partition_id -
          m_id * m_config->m_n_sub_partition_per_memory_channel);
}

void memory_partition_unit::simple_dram_model_cycle() {
  // pop completed memory request from dram and push it to dram-to-L2 queue
  // of the original sub partition
  if (!m_dram_latency_queue.empty() &&
      ((m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle) >=
       m_dram_latency_queue.front().ready_cycle)) {
    mem_fetch *mf_return = m_dram_latency_queue.front().req;
    if (mf_return->get_access_type() != L1_WRBK_ACC &&
        mf_return->get_access_type() != L2_WRBK_ACC) {
      mf_return->set_reply();

      unsigned dest_global_spid = mf_return->get_sub_partition_id();
      int dest_spid = global_sub_partition_id_to_local_id(dest_global_spid);
      assert(m_sub_partition[dest_spid]->get_id() == dest_global_spid);
      m_sub_partition[dest_spid]->l2_char_record_dram_return(
          true, m_sub_partition[dest_spid]->dram_L2_queue_full());
      if (!m_sub_partition[dest_spid]->dram_L2_queue_full()) {
        if (mf_return->get_access_type() == L1_WRBK_ACC) {
          m_sub_partition[dest_spid]->set_done(mf_return);
          delete mf_return;
        } else {
          m_sub_partition[dest_spid]->dram_L2_queue_push(mf_return);
          mf_return->set_status(
              IN_PARTITION_DRAM_TO_L2_QUEUE,
              m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
          m_arbitration_metadata.return_credit(dest_spid, mf_return);
          MEMPART_DPRINTF(
              "mem_fetch request %p return from dram to sub partition %d\n",
              mf_return, dest_spid);
        }
        m_dram_latency_queue.pop_front();
      }

    } else {
      this->set_done(mf_return);
      delete mf_return;
      m_dram_latency_queue.pop_front();
    }
  }

  // mem_fetch *mf = m_sub_partition[spid]->L2_dram_queue_top();
  // if( !m_dram->full(mf->is_write()) ) {
  // L2->DRAM queue to DRAM latency queue
  // Arbitrate among multiple L2 subpartitions
  int last_issued_partition = m_arbitration_metadata.last_borrower();
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    int spid = (p + last_issued_partition + 1) %
               m_config->m_n_sub_partition_per_memory_channel;
    if (!m_sub_partition[spid]->L2_dram_queue_empty()) {
      mem_fetch *mf = m_sub_partition[spid]->L2_dram_queue_top();
      const bool char_needs_return = requires_dram_to_l2_return(mf);
      const bool char_return_block =
          char_needs_return && m_sub_partition[spid]->dram_L2_queue_full();
      const bool char_general_credit =
          m_arbitration_metadata.has_credits(spid, false);
      const bool char_resource_credit =
          m_arbitration_metadata.has_credits(spid, !char_needs_return);
      const bool char_credit_block = !char_return_block &&
          !dram_issue_allowed(char_needs_return, false, char_general_credit,
                              !char_needs_return && char_resource_credit);
      const bool char_scheduler_block = m_dram->full(mf->is_write());
      m_sub_partition[spid]->l2_char_record_dram_issue(
          char_needs_return, !char_needs_return, char_return_block,
          char_credit_block, char_scheduler_block, m_dram->que_length());
      if (m_l2_char_dram_issue_count >=
              m_config->gpgpu_l2_char_dram_issue_hold_after_issues &&
          m_l2_char_dram_issue_hold_remaining) {
        --m_l2_char_dram_issue_hold_remaining;
        continue;
      }
      if (!can_issue_to_dram(spid, mf)) continue;
      if (m_dram->full(mf->is_write())) continue;

      if (!requires_dram_to_l2_return(mf) &&
          m_sub_partition[spid]->dram_L2_queue_full())
        m_wb_issued_while_returnq_full++;
      m_sub_partition[spid]->L2_dram_queue_pop();
      m_sub_partition[spid]->l2_char_record_dram_success(
          char_needs_return, !char_needs_return, mf->get_data_size());
      c7e_record_dram_success(char_needs_return, !char_needs_return,
                              mf->get_data_size());
      MEMPART_DPRINTF(
          "Issue mem_fetch request %p from sub partition %d to dram\n", mf,
          spid);
      dram_delay_t d;
      d.req = mf;
      d.ready_cycle = m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle +
                      m_config->dram_latency;
      m_dram_latency_queue.push_back(d);
      mf->set_status(IN_PARTITION_DRAM_LATENCY_QUEUE,
                     m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      m_arbitration_metadata.borrow_credit(
          spid, mf, !requires_dram_to_l2_return(mf));
      ++m_l2_char_dram_issue_count;
      break;  // the DRAM should only accept one request per cycle
    }
  }
  //}
}

void memory_partition_unit::dram_cycle() {
  c7e_sample_dram_cycle();
  // pop completed memory request from dram and push it to dram-to-L2 queue
  // of the original sub partition
  mem_fetch *mf_return = m_dram->return_queue_top();
  if (mf_return) {
    unsigned dest_global_spid = mf_return->get_sub_partition_id();
    int dest_spid = global_sub_partition_id_to_local_id(dest_global_spid);
    assert(m_sub_partition[dest_spid]->get_id() == dest_global_spid);
    if (mf_return->get_access_type() != L1_WRBK_ACC &&
        mf_return->get_access_type() != L2_WRBK_ACC)
      m_sub_partition[dest_spid]->l2_char_record_dram_return(
          true, m_sub_partition[dest_spid]->dram_L2_queue_full());
    if (!m_sub_partition[dest_spid]->dram_L2_queue_full()) {
      if (mf_return->get_access_type() == L1_WRBK_ACC) {
        m_sub_partition[dest_spid]->set_done(mf_return);
        delete mf_return;
      } else {
        m_sub_partition[dest_spid]->dram_L2_queue_push(mf_return);
        mf_return->set_status(IN_PARTITION_DRAM_TO_L2_QUEUE,
                              m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
        m_arbitration_metadata.return_credit(dest_spid, mf_return);
        MEMPART_DPRINTF(
            "mem_fetch request %p return from dram to sub partition %d\n",
            mf_return, dest_spid);
      }
      m_dram->return_queue_pop();
    }
  } else {
    m_dram->return_queue_pop();
  }

  m_dram->cycle();
  m_dram->dram_log(SAMPLELOG);

  // mem_fetch *mf = m_sub_partition[spid]->L2_dram_queue_top();
  // if( !m_dram->full(mf->is_write()) ) {
  // L2->DRAM queue to DRAM latency queue
  // Arbitrate among multiple L2 subpartitions
  int last_issued_partition = m_arbitration_metadata.last_borrower();
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    int spid = (p + last_issued_partition + 1) %
               m_config->m_n_sub_partition_per_memory_channel;
    if (!m_sub_partition[spid]->L2_dram_queue_empty()) {
      mem_fetch *mf = m_sub_partition[spid]->L2_dram_queue_top();
      const bool char_needs_return = requires_dram_to_l2_return(mf);
      const bool char_return_block =
          char_needs_return && m_sub_partition[spid]->dram_L2_queue_full();
      const bool char_general_credit =
          m_arbitration_metadata.has_credits(spid, false);
      const bool char_resource_credit =
          m_arbitration_metadata.has_credits(spid, !char_needs_return);
      const bool char_credit_block = !char_return_block &&
          !dram_issue_allowed(char_needs_return, false, char_general_credit,
                              !char_needs_return && char_resource_credit);
      const bool char_scheduler_block = m_dram->full(mf->is_write());
      m_sub_partition[spid]->l2_char_record_dram_issue(
          char_needs_return, !char_needs_return, char_return_block,
          char_credit_block, char_scheduler_block, m_dram->que_length());
      if (m_l2_char_dram_issue_count >=
              m_config->gpgpu_l2_char_dram_issue_hold_after_issues &&
          m_l2_char_dram_issue_hold_remaining) {
        --m_l2_char_dram_issue_hold_remaining;
        continue;
      }
      if (!can_issue_to_dram(spid, mf)) continue;
      if (m_dram->full(mf->is_write())) continue;

      if (!requires_dram_to_l2_return(mf) &&
          m_sub_partition[spid]->dram_L2_queue_full())
        m_wb_issued_while_returnq_full++;

      m_sub_partition[spid]->L2_dram_queue_pop();
      m_sub_partition[spid]->l2_char_record_dram_success(
          char_needs_return, !char_needs_return, mf->get_data_size());
      c7e_record_dram_success(char_needs_return, !char_needs_return,
                              mf->get_data_size());
      MEMPART_DPRINTF(
          "Issue mem_fetch request %p from sub partition %d to dram\n", mf,
          spid);
      dram_delay_t d;
      d.req = mf;
      d.ready_cycle = m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle +
                      m_config->dram_latency;
      m_dram_latency_queue.push_back(d);
      mf->set_status(IN_PARTITION_DRAM_LATENCY_QUEUE,
                     m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      m_arbitration_metadata.borrow_credit(
          spid, mf, !requires_dram_to_l2_return(mf));
      ++m_l2_char_dram_issue_count;
      break;  // the DRAM should only accept one request per cycle
    }
  }
  //}

  // DRAM latency queue
  if (!m_dram_latency_queue.empty() &&
      ((m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle) >=
       m_dram_latency_queue.front().ready_cycle) &&
      !m_dram->full(m_dram_latency_queue.front().req->is_write())) {
    mem_fetch *mf = m_dram_latency_queue.front().req;
    m_dram_latency_queue.pop_front();
    m_dram->push(mf);
  }
}

void memory_partition_unit::set_done(mem_fetch *mf) {
  unsigned global_spid = mf->get_sub_partition_id();
  int spid = global_sub_partition_id_to_local_id(global_spid);
  assert(m_sub_partition[spid]->get_id() == global_spid);
  if (mf->get_access_type() == L1_WRBK_ACC ||
      mf->get_access_type() == L2_WRBK_ACC) {
    m_arbitration_metadata.return_credit(spid, mf);
    MEMPART_DPRINTF(
        "mem_fetch request %p return from dram to sub partition %d\n", mf,
        spid);
  }
  m_sub_partition[spid]->set_done(mf);
}

void memory_partition_unit::set_dram_power_stats(
    unsigned &n_cmd, unsigned &n_activity, unsigned &n_nop, unsigned &n_act,
    unsigned &n_pre, unsigned &n_rd, unsigned &n_wr, unsigned &n_wr_WB,
    unsigned &n_req) const {
  m_dram->set_dram_power_stats(n_cmd, n_activity, n_nop, n_act, n_pre, n_rd,
                               n_wr, n_wr_WB, n_req);
}

void memory_partition_unit::print(FILE *fp) const {
  fprintf(fp, "Memory Partition %u: \n", m_id);
  fprintf(fp, "L2_char_credit_leak_free = %u\n",
          m_arbitration_metadata.no_credit_leak() ? 1 : 0);
  fprintf(fp, "L2_char_wb_progress_credit_use_count = %llu\n",
          m_arbitration_metadata.wb_progress_credit_use_count());
  fprintf(fp, "L2_char_wb_progress_credit_current = %u\n",
          m_arbitration_metadata.wb_progress_credit_current());
  fprintf(fp, "L2_char_wb_progress_credit_max = %u\n",
          m_arbitration_metadata.wb_progress_credit_max());
  fprintf(fp, "L2_char_wb_progress_credit_limit = %u\n",
          m_arbitration_metadata.wb_progress_credit_limit());
  fprintf(fp, "L2_char_wb_issued_while_returnq_full = %llu\n",
          m_wb_issued_while_returnq_full);
  fprintf(fp, "L2_char_wb_head_while_returnq_full = %llu\n",
          m_wb_head_while_returnq_full);
  fprintf(fp, "L2_char_read_head_while_returnq_full = %llu\n",
          m_read_head_while_returnq_full);
  fprintf(fp, "L2_char_read_issue_blocked_while_returnq_full = %llu\n",
          m_read_issue_blocked_while_returnq_full);
  if (m_config->m_L2_config.ep_l2_b0_stats_enabled()) {
    const unsigned long long bytes = m_c7e_successful_read_bytes +
                                     m_c7e_successful_write_bytes;
    const unsigned long long capacity =
        m_c7e_dram_cycles * (unsigned long long)m_config->dram_atom_size;
    fprintf(fp,
            "EPL2DRAMV1|scope=application|channel=%u|dram_cycles=%llu|"
            "scheduler_occ_avg=%llu|scheduler_occ_max=%u|scheduler_full_cycles=%llu|"
            "returnq_occ_avg=%llu|returnq_occ_max=%u|returnq_full_cycles=%llu|"
            "successful_read_issues=%llu|successful_write_issues=%llu|"
            "successful_read_bytes=%llu|successful_write_bytes=%llu|"
            "bandwidth_util_numerator_bytes=%llu|bandwidth_util_denominator_bytes=%llu|"
            "bandwidth_util=%0.9f\n",
            m_id, m_c7e_dram_cycles,
            m_c7e_dram_cycles ? m_c7e_scheduler_occ_sum / m_c7e_dram_cycles : 0,
            m_c7e_scheduler_occ_max, m_c7e_scheduler_full_cycles,
            m_c7e_dram_cycles ? m_c7e_returnq_occ_sum / m_c7e_dram_cycles : 0,
            m_c7e_returnq_occ_max, m_c7e_returnq_full_cycles,
            m_c7e_successful_read_issues, m_c7e_successful_write_issues,
            m_c7e_successful_read_bytes, m_c7e_successful_write_bytes,
            bytes, capacity, capacity ? (double)bytes / capacity : 0.0);
  }
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel;
       p++) {
    m_sub_partition[p]->print(fp);
  }
  fprintf(fp, "In Dram Latency Queue (total = %zd): \n",
          m_dram_latency_queue.size());
  for (std::list<dram_delay_t>::const_iterator mf_dlq =
           m_dram_latency_queue.begin();
       mf_dlq != m_dram_latency_queue.end(); ++mf_dlq) {
    mem_fetch *mf = mf_dlq->req;
    fprintf(fp, "Ready @ %llu - ", mf_dlq->ready_cycle);
    if (mf)
      mf->print(fp);
    else
      fprintf(fp, " <NULL mem_fetch?>\n");
  }
  m_dram->print(fp);
}

void memory_partition_unit::print_ep_l2_b0_snapshot(FILE *fp,
                                                     unsigned long long uid) const {
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel; ++p)
    m_sub_partition[p]->print_ep_l2_b0_snapshot(fp, uid);
}

void memory_partition_unit::begin_ep_l2_b0_kernel(unsigned long long uid) {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel; ++p)
    m_sub_partition[p]->begin_ep_l2_b0_kernel(uid);
}

void memory_partition_unit::end_ep_l2_b0_kernel(FILE *fp,
                                                 unsigned long long uid) {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  for (unsigned p = 0; p < m_config->m_n_sub_partition_per_memory_channel; ++p)
    m_sub_partition[p]->end_ep_l2_b0_kernel(fp, uid);
}

memory_sub_partition::memory_sub_partition(unsigned sub_partition_id,
                                           const memory_config *config,
                                           class memory_stats_t *stats,
                                           class gpgpu_sim *gpu) {
  m_id = sub_partition_id;
  m_config = config;
  m_stats = stats;
  m_gpu = gpu;
  m_memcpy_cycle_offset = 0;

  assert(m_id < m_config->m_n_mem_sub_partition);

  char L2c_name[32];
  snprintf(L2c_name, 32, "L2_bank_%03d", m_id);
  m_L2interface = new L2interface(this);
  m_mf_allocator = new partition_mf_allocator(config);

  if (!m_config->m_L2_config.disabled())
    m_L2cache = new l2_cache(L2c_name, m_config->m_L2_config, -1, -1,
                             m_L2interface, m_mf_allocator,
                             IN_PARTITION_L2_MISS_QUEUE, gpu, L2_GPU_CACHE);

  unsigned int icnt_L2;
  unsigned int L2_dram;
  unsigned int dram_L2;
  unsigned int L2_icnt;
  sscanf(m_config->gpgpu_L2_queue_config, "%u:%u:%u:%u", &icnt_L2, &L2_dram,
         &dram_L2, &L2_icnt);
  m_icnt_L2_queue = new fifo_pipeline<mem_fetch>("icnt-to-L2", 0, icnt_L2);
  m_L2_dram_queue = new fifo_pipeline<mem_fetch>("L2-to-dram", 0, L2_dram);
  m_dram_L2_queue = new fifo_pipeline<mem_fetch>("dram-to-L2", 0, dram_L2);
  m_L2_icnt_queue = new fifo_pipeline<mem_fetch>("L2-to-icnt", 0, L2_icnt);
  m_l2_char_returnq_hold_remaining =
      config->gpgpu_l2_char_returnq_hold_cycles;
  m_l2_char_collector = 0;
  for (unsigned i = 0; i < 4; ++i) m_l2_char_l2dram_class[i] = 0;
  m_ep_l2_lower_read_issue_count = 0;
  m_ep_l2_payload_identity_lower_issue_count = 0;
  m_ep_l2_last_preview_block_reason = mshr_table::EP_L2_BLOCK_NONE;
  m_ep_l2_b0_window_start_cycle = 0;
  m_ep_l2_b0_window_started = false;
  // C7d samples the maintained tag-state counters too. Enabling that
  // sidecar is observational and avoids a per-cycle tag-array scan; it is
  // independent of whether legacy L2CHARV1 output is requested.
  if (!m_config->m_L2_config.disabled() &&
      (config->gpgpu_l2_char_enable ||
       m_config->m_L2_config.ep_l2_b0_stats_enabled()))
    m_L2cache->l2_char_tracking_enable();
  if (!m_config->m_L2_config.disabled() && config->gpgpu_l2_char_enable) {
    m_l2_char_collector = new l2_char_collector(
        m_id, m_config->m_L2_config.m_nset, m_config->m_L2_config.m_assoc,
        m_L2cache->mshr_entry_capacity(), m_L2cache->mshr_merge_capacity(),
        m_L2cache->miss_queue_capacity(), m_L2_dram_queue->get_max_len(),
        m_dram_L2_queue->get_max_len(), m_L2_icnt_queue->get_max_len(),
        m_icnt_L2_queue->get_max_len(), config->gpgpu_l2_char_window,
        config->gpgpu_l2_char_set_detail, config->gpgpu_l2_char_emit_windows);
  }
  wb_addr = -1;
}

memory_sub_partition::~memory_sub_partition() {
  delete m_icnt_L2_queue;
  delete m_L2_dram_queue;
  delete m_dram_L2_queue;
  delete m_L2_icnt_queue;
  delete m_L2cache;
  delete m_L2interface;
  delete m_l2_char_collector;
}

unsigned memory_sub_partition::l2_char_queue_class(const mem_fetch *mf) {
  if (mf->get_access_type() == L2_WRBK_ACC ||
      mf->get_access_type() == L1_WRBK_ACC) return 1;
  if (!mf->get_is_write()) return 0;
  return 2;
}

void memory_sub_partition::l2_char_record_l2dram_push(mem_fetch *mf) {
  if (!m_l2_char_collector) return;
  const unsigned klass = l2_char_queue_class(mf);
  ++m_l2_char_l2dram_class[klass];
  m_l2_char_collector->record_l2dram_push_class(klass, mf->get_data_size());
}

void memory_sub_partition::ep_l2_record_lower_issue(mem_fetch *mf) {
  if (m_config->m_L2_config.ep_l2_descriptor_mode() && !mf->get_is_write())
    ++m_ep_l2_lower_read_issue_count;
  if (m_config->m_L2_config.m_ep_l2_payload_mode &&
      mf->has_ep_l2_payload_identity())
    ++m_ep_l2_payload_identity_lower_issue_count;
}

void memory_sub_partition::l2_char_record_l2dram_pop(mem_fetch *mf) {
  if (!m_l2_char_collector) return;
  const unsigned klass = l2_char_queue_class(mf);
  assert(m_l2_char_l2dram_class[klass]);
  --m_l2_char_l2dram_class[klass];
  m_l2_char_collector->record_l2dram_pop_class(klass);
}

void memory_sub_partition::l2_char_sample(unsigned long long cycle) {
  if (!m_l2_char_collector) return;
  l2_char_cycle_sample s;
  m_L2cache->l2_char_storage_snapshot(s.storage.reserved, s.storage.dirty,
                                       s.storage.valid, s.storage.reserved_by_set);
  for (std::vector<unsigned>::const_iterator it = s.storage.reserved_by_set.begin();
       it != s.storage.reserved_by_set.end(); ++it) {
    if (*it > s.storage.max_reserved_set) s.storage.max_reserved_set = *it;
    if (*it == m_config->m_L2_config.m_assoc) ++s.storage.all_reserved_sets;
  }
  s.mshr_entries = m_L2cache->mshr_entries_used();
  s.mshr_ready_entries = m_L2cache->mshr_ready_entries();
  s.mshr_targets = m_L2cache->mshr_targets_used();
  s.mshr_ready_targets = m_L2cache->mshr_ready_targets();
  s.max_merge_depth = m_L2cache->mshr_max_targets_on_one_entry();
  std::vector<new_addr_type> addresses;
  std::vector<unsigned> targets;
  std::vector<bool> ready;
  m_L2cache->l2_char_mshr_states(addresses, targets, ready);
  for (unsigned i = 0; i < addresses.size(); ++i) {
    s.mshr_states.push_back(l2_char_mshr_state(addresses[i], targets[i], ready[i]));
    if (targets[i] == m_L2cache->mshr_merge_capacity()) ++s.merge_limit_entries;
  }
  unsigned other = 0;
  m_L2cache->miss_queue_class_counts(s.missq_demand, s.missq_wb, other);
  s.missq_other_write = other;
  s.missq = m_L2cache->miss_queue_occupancy();
  s.l2dramq = m_L2_dram_queue->get_n_element();
  s.draml2q = m_dram_L2_queue->get_n_element();
  s.l2icntq = m_L2_icnt_queue->get_n_element();
  s.icntl2q = m_icnt_L2_queue->get_n_element();
  s.rop = m_rop.size();
  // The overall sampling point remains here.  Port utilization itself is a
  // pre-replenish cache-cycle observation, latched by baseline_cache::cycle()
  // alongside the native cache_stats sample.
  s.data_port_busy = m_L2cache->l2_char_data_port_busy_snapshot();
  s.fill_port_busy = m_L2cache->l2_char_fill_port_busy_snapshot();
  m_l2_char_collector->observe_queue_classes(
      s.missq, s.missq_demand, s.missq_wb, s.missq_other_write, s.l2dramq);
  m_l2_char_collector->sample_cycle(cycle, s);
}

void memory_sub_partition::cache_cycle(unsigned cycle) {
  // L2 fill responses
  if (!m_config->m_L2_config.disabled()) {
    if (m_l2_char_collector)
      m_l2_char_collector->record_mshr_response(
          m_L2cache->access_ready(),
          m_L2cache->access_ready() && m_L2_icnt_queue->full());
    if (m_L2cache->access_ready() && !m_L2_icnt_queue->full()) {
      // Keep the EP-L2 descriptor live until this response has actually been
      // accepted by the L2->ICNT queue below.
      mem_fetch *mf = m_L2cache->peek_next_access();
      if (mf->get_access_type() !=
          L2_WR_ALLOC_R) {  // Don't pass write allocate read request back to
                            // upper level cache
        mf->set_reply();
        mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE,
                       m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
        m_L2_icnt_queue->push(mf);
      } else {
        if (m_config->m_L2_config.m_write_alloc_policy == FETCH_ON_WRITE) {
          mem_fetch *original_wr_mf = mf->get_original_wr_mf();
          assert(original_wr_mf);
          original_wr_mf->set_reply();
          original_wr_mf->set_status(
              IN_PARTITION_L2_TO_ICNT_QUEUE,
              m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
          m_L2_icnt_queue->push(original_wr_mf);
        }
        m_request_tracker.erase(mf);
        delete mf;
      }
      m_L2cache->commit_next_access();
    }
  }

  // DRAM to L2 (texture) and icnt (not texture)
  if (!m_dram_L2_queue->empty() && m_l2_char_returnq_hold_remaining) {
    // Directed closeout hook only: preserve an actual returned response in
    // the ReturnQ. Default zero leaves the production path untouched.
    --m_l2_char_returnq_hold_remaining;
  } else if (!m_dram_L2_queue->empty()) {
    mem_fetch *mf = m_dram_L2_queue->top();
    if (!m_config->m_L2_config.disabled() && m_L2cache->waiting_for_fill(mf)) {
      // Target payload modes replace the legacy FillPort with the resident
      // payload RAM write port. The returned transaction's landing identity
      // selects the physical target slot; no hard-coded slot may be used.
      const ep_l2_payload_store::request_result fill_result =
          m_L2cache->ep_l2_payload_fill_request(
              mf, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      const bool fill_ready = fill_result == ep_l2_payload_store::GRANTED;
      if (m_l2_char_collector)
        m_l2_char_collector->record_fill(true, !fill_ready);
      if (!fill_ready && m_config->m_L2_config.m_ep_l2_payload_mode) {
        ++m_ep_l2_b0_accum.payload_block;
        if (fill_result == ep_l2_payload_store::BANK_TRUE_CONTENTION)
          ++m_ep_l2_b0_accum.bank_block;
        else if (fill_result == ep_l2_payload_store::LEGACY_PORT_BUSY)
          ++m_ep_l2_b0_accum.payload_service_port_denial;
      }
      if (fill_ready) {
        mf->set_status(IN_PARTITION_L2_FILL_QUEUE,
                       m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
        m_L2cache->fill(mf, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle +
                                m_memcpy_cycle_offset);
        m_dram_L2_queue->pop();
      }
    } else if (!m_L2_icnt_queue->full()) {
      if (mf->is_write() && mf->get_type() == WRITE_ACK)
        mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE,
                       m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      m_L2_icnt_queue->push(mf);
      m_dram_L2_queue->pop();
    }
  }

  // prior L2 misses inserted into m_L2_dram_queue here
  if (!m_config->m_L2_config.disabled()) {
    if (m_l2_char_collector) {
      const bool lower_eligible = !m_L2cache->miss_queue_empty();
      m_l2_char_collector->record_lower_drain(
          lower_eligible, lower_eligible && m_L2_dram_queue->full());
    }
    m_L2cache->cycle();
  }

  // This is the frozen sampling point: prior-cycle drains and fills have
  // completed, but the current frontend head has not been admitted yet.
  l2_char_sample(m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);

  // New L2 texture accesses and/or non-texture accesses.  A full L2-to-DRAM
  // FIFO is not a frontend prerequisite: hits, MSHR merges, and locally
  // absorbed writes must only wait for resources they actually consume.
  if (!m_icnt_L2_queue->empty()) {
    mem_fetch *mf = m_icnt_L2_queue->top();
    if (!m_config->m_L2_config.disabled() &&
        ((m_config->m_L2_texure_only && mf->istexture()) ||
         (!m_config->m_L2_texure_only))) {
      // L2 is enabled and access is for L2
      l2_access_plan plan;
      m_L2cache->preview_access(mf->get_addr(), mf, plan);
      if (plan.exact)
        m_ep_l2_last_preview_block_reason = plan.ep_l2_mshr_block_reason;
      if (plan.exact && plan.victim_dirty)
        m_l2_char_stats.dirty_victim_preview_count++;
      const unsigned missq_occupancy_before = m_L2cache->miss_queue_occupancy();
      const unsigned mshr_entries_before = m_L2cache->mshr_entries_used();
      const bool missq_full_before =
          !m_L2cache->miss_queue_has_slots(1);
      const bool missq_one_slot_before =
          m_L2cache->miss_queue_has_slots(1) &&
          !m_L2cache->miss_queue_has_slots(2);
      bool admit = false;
      l2_admission_inputs admission;
      if (plan.exact) {
        // C7d exact target telemetry is sourced from the same non-mutating
        // production preview used for admission.  None of these counters are
        // read by the controller.
        const bool line_alloc =
            plan.probe_status == MISS || plan.probe_status == SECTOR_MISS ||
            plan.ep_l2_tag_set_all_reserved || plan.ep_l2_wad_full;
        if (line_alloc) ++m_ep_l2_b0_accum.line_alloc_eligible;
        // C7e denominator: only a true new tag-way allocation, never a
        // sector miss that lands in an already-resident line.
        if (plan.probe_status == MISS) ++m_ep_l2_b0_accum.c7e_tag_way_alloc_need;
        if (plan.ep_l2_tag_set_all_reserved) {
          ++m_ep_l2_b0_accum.line_alloc_block;
          ++m_ep_l2_b0_accum.tag_set_all_reserved_block;
          ++m_ep_l2_b0_accum.c7e_tag_way_alloc_block;
        }
        // These semantic demands deliberately do not depend on the selected
        // full_reason priority below.
        if (plan.needs_new_mshr) ++m_ep_l2_b0_accum.c7e_line_mshr_need;
        if (plan.needs_new_mshr || plan.needs_mshr_merge)
          ++m_ep_l2_b0_accum.c7e_descriptor_need;
        if (plan.needs_mshr_merge)
          ++m_ep_l2_b0_accum.c7e_per_address_cap_check;
        if (plan.needs_new_mshr || plan.needs_mshr_merge) {
          switch (plan.ep_l2_mshr_block_reason) {
            case mshr_table::EP_L2_BLOCK_LINE_MSHR_FULL:
              ++m_ep_l2_b0_accum.line_mshr_alloc_eligible;
              ++m_ep_l2_b0_accum.line_mshr_full_block;
              break;
            case mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL:
              ++m_ep_l2_b0_accum.descriptor_alloc_eligible;
              ++m_ep_l2_b0_accum.descriptor_pool_full_block;
              break;
            case mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP:
              ++m_ep_l2_b0_accum.per_address_cap_eligible;
              ++m_ep_l2_b0_accum.per_address_cap_block;
              break;
            case mshr_table::EP_L2_BLOCK_NONE:
              if (plan.needs_new_mshr)
                ++m_ep_l2_b0_accum.line_mshr_alloc_eligible;
              if (plan.needs_new_mshr || plan.needs_mshr_merge)
                ++m_ep_l2_b0_accum.descriptor_alloc_eligible;
              if (plan.needs_mshr_merge)
                ++m_ep_l2_b0_accum.per_address_cap_eligible;
              break;
          }
        }
        if (plan.ep_l2_wad_full) ++m_ep_l2_b0_accum.wad_full_events;
        if (plan.ep_l2_wad_same_address_hazard) {
          ++m_ep_l2_b0_accum.wad_hazard_events;
          ++m_ep_l2_b0_accum.wad_hazard_wait_cycles;
        }
        admission.line_available = plan.probe_status != RESERVATION_FAIL;
        admission.needs_new_mshr = plan.needs_new_mshr;
        admission.new_mshr_available = plan.mshr_entry_available;
        admission.needs_mshr_merge = plan.needs_mshr_merge;
        admission.mshr_merge_available = plan.mshr_merge_available;
        admission.missq_available =
            m_L2cache->miss_queue_has_slots(plan.new_missq_entries);
        admission.needs_data_port = plan.needs_data_port;
        admission.data_port_available = m_L2cache->data_port_free();
        admission.needs_response_slot = plan.needs_immediate_response_slot;
        admission.response_slot_available = !m_L2_icnt_queue->full();
        admit = l2_admission_allowed(admission);
        if (m_l2_char_collector) {
          unsigned eligible = 0;
          // RESERVATION_FAIL is the production form of "all replacement
          // ways are reserved".  It is still an allocation attempt, and
          // must share the LINE_ALLOC denominator with ordinary misses.
          if (plan.probe_status == MISS || plan.probe_status == SECTOR_MISS ||
              plan.probe_status == RESERVATION_FAIL)
            eligible |= 1u << L2_BLOCK_LINE_ALLOC;
          if (plan.needs_new_mshr) eligible |= 1u << L2_BLOCK_MSHR_NEW;
          if (plan.needs_mshr_merge) eligible |= 1u << L2_BLOCK_MSHR_MERGE;
          if (plan.new_missq_entries) eligible |= 1u << L2_BLOCK_MISSQ;
          if (plan.needs_data_port) eligible |= 1u << L2_BLOCK_DATA_PORT;
          if (plan.needs_immediate_response_slot) eligible |= 1u << L2_BLOCK_RESPQ;
          // l2_admission_inputs carries availability values for every
          // resource.  A resource contributes to its characterization
          // denominator only when this request actually needs it.
          const unsigned blocked = (admit ? 0 :
              static_cast<unsigned>(l2_admission_blockers(admission))) &
              eligible;
          m_l2_char_collector->record_frontend(
              mf, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle, eligible,
              blocked);
        }
      } else {
        // Preserve the historical shared controller behavior for non-QV100
        // policies until they receive their own reviewed exact plan.
        admit = !m_L2_dram_queue->full() && !m_L2_icnt_queue->full() &&
                m_L2cache->data_port_free();
      }
      // An exact preview that is not admissible is a real controller stall,
      // even though access() is intentionally not called.  Record it here so
      // blocker accounting describes the production admission decision rather
      // than only an unexpected reservation failure after admission.
      if (plan.exact && !admit) {
        unsigned long long blockers = l2_admission_blockers(admission);
        if (!blockers) blockers = 1ULL << L2_BLOCK_OTHER;
        m_l2_char_stats.record_blockers(
            mf, blockers, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle,
            m_l2_block_state);
        if (plan.ep_l2_mshr_block_reason != mshr_table::EP_L2_BLOCK_NONE)
          ++m_ep_l2_b0_accum.descriptor_block;
        // Compatibility fields remain coarse; C7d consumers must use the
        // explicitly named exact fields above instead of reinterpreting them.
        if (plan.ep_l2_wad_full || plan.ep_l2_wad_same_address_hazard)
          ++m_ep_l2_b0_accum.wad_block;
        if (plan.new_missq_entries && !admission.missq_available)
          ++m_ep_l2_b0_accum.missq_full_block;
        if (plan.will_send_lower_read && m_L2_dram_queue->full()) {
          ++m_ep_l2_b0_accum.lower_block;
          ++m_ep_l2_b0_accum.l2_to_dram_full_block;
        }

        if (plan.needs_data_port && !admission.data_port_available &&
            plan.is_read && plan.probe_status == HIT)
          m_l2_char_stats.dataport_hit_block_cycles++;

        // A dirty victim consumes two MissQ entries (writeback plus demand).
        // When precisely one entry remains, refusal must be completely
        // side-effect free: previewing again must see the same cache and
        // queue state because access() was never called.
        if (plan.victim_dirty && plan.new_missq_entries == 2 &&
            missq_one_slot_before) {
          m_l2_char_stats.missq_dirty_miss_block_one_slot++;
          l2_access_plan retry_plan;
          m_L2cache->preview_access(mf->get_addr(), mf, retry_plan);
          const bool unchanged =
              m_L2cache->miss_queue_occupancy() == missq_occupancy_before &&
              m_L2cache->mshr_entries_used() == mshr_entries_before &&
              retry_plan.probe_status == plan.probe_status &&
              retry_plan.cache_index == plan.cache_index &&
              retry_plan.victim_valid == plan.victim_valid &&
              retry_plan.victim_dirty == plan.victim_dirty &&
              retry_plan.needs_new_mshr == plan.needs_new_mshr &&
              retry_plan.needs_mshr_merge == plan.needs_mshr_merge &&
              retry_plan.new_missq_entries == plan.new_missq_entries &&
              retry_plan.will_send_lower_read == plan.will_send_lower_read &&
              retry_plan.ep_l2_mshr_block_reason ==
                  plan.ep_l2_mshr_block_reason &&
              retry_plan.ep_l2_needs_lower_read == plan.ep_l2_needs_lower_read &&
              retry_plan.will_send_writeback == plan.will_send_writeback;
          if (unchanged)
            m_l2_char_stats.missq_dirty_block_no_mutation++;
          else
            m_l2_char_stats.missq_dirty_block_partial_mutation++;
#ifndef NDEBUG
          assert(unchanged);
#endif
        }
      }
      if (admit) {
        const bool lowerq_full_before = m_L2_dram_queue->full();
        const bool respq_full_before = m_L2_icnt_queue->full();
        const bool dataport_busy_before = !m_L2cache->data_port_free();
        if (plan.exact &&
            (lowerq_full_before || respq_full_before ||
             dataport_busy_before)) {
          // Count only admissions that the official coarse gate would have
          // rejected; this is the low-pressure equivalence activation signal.
          m_l2_char_stats.corrected_path_activation_count++;
          if (lowerq_full_before)
            m_l2_char_stats.corrected_path_lowerq_activation_count++;
          if (respq_full_before)
            m_l2_char_stats.corrected_path_respq_activation_count++;
          if (dataport_busy_before)
            m_l2_char_stats.corrected_path_dataport_activation_count++;
        }
        if (plan.exact && dataport_busy_before) {
          if (plan.is_read && plan.needs_new_mshr &&
              plan.will_send_lower_read && !plan.victim_dirty)
            m_l2_char_stats.dataport_clean_miss_admit_while_busy++;
          if (plan.is_read && plan.needs_mshr_merge)
            m_l2_char_stats.dataport_mshr_merge_admit_while_busy++;
        }
        if (plan.exact && plan.is_read && plan.needs_mshr_merge &&
            missq_full_before)
          m_l2_char_stats.missq_merge_admit_while_full++;
        if (plan.exact && plan.is_read && plan.needs_new_mshr &&
            plan.new_missq_entries == 1 && missq_one_slot_before)
          m_l2_char_stats.missq_clean_miss_admit_one_slot++;
        if (plan.exact && plan.is_read && plan.needs_new_mshr &&
            plan.victim_dirty && plan.new_missq_entries == 2)
          m_l2_char_stats.missq_dirty_miss_admit_two_slots++;

        unsigned missq_before = missq_occupancy_before;
        std::list<cache_event> events;
        enum cache_request_status status =
            m_L2cache->access(mf->get_addr(), mf,
                              m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle +
                                  m_memcpy_cycle_offset,
                              events);
        if (status == RESERVATION_FAIL &&
            m_config->m_L2_config.m_ep_l2_payload_mode) {
          ++m_ep_l2_b0_accum.payload_block;
          if (m_L2cache->ep_l2_last_payload_bank_contention())
            ++m_ep_l2_b0_accum.bank_block;
          else if (m_L2cache->ep_l2_last_payload_service_port_denial())
            ++m_ep_l2_b0_accum.payload_service_port_denial;
        }
        m_l2_block_state.erase(mf);
        if (m_l2_char_collector) {
          m_l2_char_collector->clear_frontend_request(mf);
          if (plan.needs_data_port) {
            const unsigned source = plan.probe_status == HIT ? 0 :
                                    plan.victim_dirty ? 1 : 2;
            m_l2_char_collector->record_data_port_accept(source);
          }
          cache_event wb_event(WRITE_BACK_REQUEST_SENT);
          if (plan.will_send_writeback && was_writeback_sent(events, wb_event)) {
            m_l2_char_collector->record_wb_generated(plan.victim_modified_bytes);
          }
        }
        // A reservation failure is deliberately not an accepted commit: the
        // preview/commit predicate encodes that contract and must therefore
        // only be checked after a non-reservation-fail access.  This applies
        // to both legacy and banked payload modes.
        if (plan.exact && status != RESERVATION_FAIL) {
          cache_event writeback_event(WRITE_BACK_REQUEST_SENT);
          bool preview_matches = l2_preview_commit_matches(
              plan.will_send_lower_read, plan.will_send_lower_write,
              plan.will_send_writeback, plan.new_missq_entries,
              status == RESERVATION_FAIL, was_read_sent(events),
              was_write_sent(events),
              was_writeback_sent(events, writeback_event),
              m_L2cache->miss_queue_occupancy() - missq_before);
          if (!preview_matches) m_l2_char_stats.preview_commit_mismatch++;
#ifndef NDEBUG
          if (!preview_matches) {
            fprintf(stderr,
                    "L2 preview/commit mismatch: status=%d expected "
                    "read=%d write=%d wb=%d missq=%u actual "
                    "read=%d write=%d wb=%d missq=%u\n",
                    status, plan.will_send_lower_read,
                    plan.will_send_lower_write, plan.will_send_writeback,
                    plan.new_missq_entries, was_read_sent(events),
                    was_write_sent(events),
                    was_writeback_sent(events, writeback_event),
                    m_L2cache->miss_queue_occupancy() - missq_before);
          }
          assert(preview_matches);
#endif
        }
        bool write_sent = was_write_sent(events);
        bool read_sent = was_read_sent(events);
        MEM_SUBPART_DPRINTF("Probing L2 cache Address=%llx, status=%u\n",
                            mf->get_addr(), status);

        if (status == HIT) {
          if (!write_sent) {
            // L2 cache replies
            assert(!read_sent);
            if (mf->get_access_type() == L1_WRBK_ACC) {
              m_request_tracker.erase(mf);
              delete mf;
            } else {
              mf->set_reply();
              mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE,
                             m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
              m_L2_icnt_queue->push(mf);
            }
            m_icnt_L2_queue->pop();
          } else {
            assert(write_sent);
            m_icnt_L2_queue->pop();
          }
        } else if (status != RESERVATION_FAIL) {
          if (mf->is_write() &&
              (m_config->m_L2_config.m_write_alloc_policy == FETCH_ON_WRITE ||
               m_config->m_L2_config.m_write_alloc_policy ==
                   LAZY_FETCH_ON_READ) &&
              !was_writeallocate_sent(events)) {
            if (mf->get_access_type() == L1_WRBK_ACC) {
              m_request_tracker.erase(mf);
              delete mf;
            } else if (m_config->m_L2_config.get_write_policy() == WRITE_BACK) {
              mf->set_reply();
              mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE,
                             m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
              m_L2_icnt_queue->push(mf);
            }
          }
          // L2 cache accepted request
          m_icnt_L2_queue->pop();
        } else {
          assert(!write_sent);
          assert(!read_sent);
          // L2 cache lock-up: will try again next cycle.
        }
      }
    } else {
      // L2 is disabled or non-texture access to texture-only L2
      mf->set_status(IN_PARTITION_L2_TO_DRAM_QUEUE,
                     m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      m_L2_dram_queue->push(mf);
      m_l2_block_state.erase(mf);
      if (m_l2_char_collector) m_l2_char_collector->clear_frontend_request(mf);
      m_icnt_L2_queue->pop();
    }
  }

  // ROP delay queue
  if (m_l2_char_collector && !m_rop.empty() &&
      (cycle >= m_rop.front().ready_cycle))
    m_l2_char_collector->record_rop(true, m_icnt_L2_queue->full());
  if (!m_rop.empty() && (cycle >= m_rop.front().ready_cycle) &&
      !m_icnt_L2_queue->full()) {
    mem_fetch *mf = m_rop.front().req;
    m_rop.pop();
    m_icnt_L2_queue->push(mf);
    mf->set_status(IN_PARTITION_ICNT_TO_L2_QUEUE,
                   m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
  }

  if (!m_config->m_L2_config.disabled()) {
    unsigned missq_demand, missq_wb, missq_other_write;
    m_L2cache->miss_queue_class_counts(missq_demand, missq_wb,
                                        missq_other_write);
    m_l2_char_stats.sample(
        m_L2cache->mshr_entries_used(), m_L2cache->mshr_targets_used(),
        m_L2cache->mshr_ready_entries(), m_L2cache->mshr_ready_targets(),
        m_L2cache->miss_queue_occupancy(), missq_demand, missq_wb,
        m_L2_dram_queue->get_n_element(), m_dram_L2_queue->get_n_element(),
        m_L2_icnt_queue->get_n_element(), m_icnt_L2_queue->get_n_element(),
        m_rop.size(), !m_L2cache->data_port_free(),
        !m_L2cache->fill_port_free(),
        !m_L2cache->miss_queue_has_slots(1), m_dram_L2_queue->full(),
        m_L2_icnt_queue->full(), m_icnt_L2_queue->full());
    ep_l2_b0_sample(m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
  }
}

bool memory_sub_partition::full() const { return m_icnt_L2_queue->full(); }

bool memory_sub_partition::full(unsigned size) const {
  return m_icnt_L2_queue->is_avilable_size(size);
}

bool memory_sub_partition::L2_dram_queue_empty() const {
  return m_L2_dram_queue->empty();
}

class mem_fetch *memory_sub_partition::L2_dram_queue_top() const {
  return m_L2_dram_queue->top();
}

void memory_sub_partition::L2_dram_queue_pop() {
  mem_fetch *mf = m_L2_dram_queue->top();
  l2_char_record_l2dram_pop(mf);
  m_L2_dram_queue->pop();
}

void memory_sub_partition::l2_char_record_dram_issue(
    bool is_read, bool is_wb, bool return_block, bool credit_block,
    bool scheduler_block, unsigned scheduler_occupancy) {
  if (m_l2_char_collector)
    m_l2_char_collector->record_dram_issue(is_read, is_wb, return_block,
                                            credit_block, scheduler_block);
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  ++m_ep_l2_b0_accum.dram_issue_eligible;
  ++m_ep_l2_b0_accum.c7e_dram_issue_attempt;
  if (is_read) ++m_ep_l2_b0_accum.dram_read_issues;
  if (is_wb) ++m_ep_l2_b0_accum.dram_write_issues;
  if (return_block) ++m_ep_l2_b0_accum.dram_returnq_block;
  if (return_block) ++m_ep_l2_b0_accum.c7e_dram_to_l2_return_path_block;
  if (credit_block) ++m_ep_l2_b0_accum.dram_credit_block;
  if (scheduler_block) ++m_ep_l2_b0_accum.dram_scheduler_full_block;
  if (scheduler_block) ++m_ep_l2_b0_accum.c7e_dram_scheduler_full_observed;
  if (scheduler_block && !return_block && !credit_block)
    ++m_ep_l2_b0_accum.c7e_dram_scheduler_causal_block;
  m_ep_l2_b0_accum.dram_scheduler_occ_sum += scheduler_occupancy;
  ++m_ep_l2_b0_accum.dram_scheduler_occ_samples;
  m_ep_l2_b0_accum.dram_scheduler_occ_max =
      std::max(m_ep_l2_b0_accum.dram_scheduler_occ_max, scheduler_occupancy);
  ++m_ep_l2_b0_accum.dram_scheduler_occ_hist[
      std::min<unsigned>(scheduler_occupancy,
                         m_ep_l2_b0_accum.dram_scheduler_occ_hist.size() - 1)];
}

void memory_sub_partition::l2_char_record_dram_success(bool is_read,
                                                         bool is_wb,
                                                         unsigned bytes) {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  if (is_read) {
    ++m_ep_l2_b0_accum.c7e_dram_successful_read_issues;
    m_ep_l2_b0_accum.c7e_dram_successful_read_bytes += bytes;
  }
  if (is_wb) {
    ++m_ep_l2_b0_accum.c7e_dram_successful_write_issues;
    m_ep_l2_b0_accum.c7e_dram_successful_write_bytes += bytes;
  }
}

void memory_sub_partition::l2_char_record_dram_return(bool eligible,
                                                       bool blocked) {
  if (m_l2_char_collector) m_l2_char_collector->record_dram_return(eligible, blocked);
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  if (eligible) ++m_ep_l2_b0_accum.dram_return_eligible;
  if (blocked) ++m_ep_l2_b0_accum.dram_to_l2_full_block;
}

bool memory_sub_partition::dram_L2_queue_full() const {
  return m_dram_L2_queue->full();
}

bool memory_sub_partition::dram_L2_queue_empty() const {
  return m_dram_L2_queue->empty();
}

void memory_sub_partition::dram_L2_queue_push(class mem_fetch *mf) {
  m_dram_L2_queue->push(mf);
}

void memory_sub_partition::l2_char_hold_returnq(unsigned cycles) {
  assert(!m_dram_L2_queue->empty());
  m_l2_char_returnq_hold_remaining = cycles;
}

void memory_sub_partition::print_cache_stat(unsigned &accesses,
                                            unsigned &misses) const {
  FILE *fp = stdout;
  if (!m_config->m_L2_config.disabled()) m_L2cache->print(fp, accesses, misses);
}

void memory_sub_partition::print(FILE *fp) const {
  if (!m_request_tracker.empty()) {
    fprintf(fp, "Memory Sub Parition %u: pending memory requests:\n", m_id);
    for (std::set<mem_fetch *>::const_iterator r = m_request_tracker.begin();
         r != m_request_tracker.end(); ++r) {
      mem_fetch *mf = *r;
      if (mf)
        mf->print(fp);
      else
        fprintf(fp, " <NULL mem_fetch?>\n");
    }
  }
  if (!m_config->m_L2_config.disabled()) m_L2cache->display_state(fp);
  if (!m_config->m_L2_config.disabled()) print_l2_char_stats(fp);
}

void memory_sub_partition::print_l2_char_stats(FILE *fp) const {
  m_l2_char_stats.print(fp, m_id);
  if (m_l2_char_collector)
    m_l2_char_collector->print(
        fp, m_L2cache->l2_char_native_data_busy_cycles(),
        m_L2cache->l2_char_native_fill_busy_cycles(),
        m_L2cache->l2_char_native_port_samples());
  fprintf(fp, "L2_char_resource_leak_free = %u\n",
          l2_char_no_resource_leak() ? 1 : 0);
  print_ep_l2_b0_snapshot(fp, (unsigned long long)-1);
}

void memory_sub_partition::ep_l2_b0_accum::sample(
    unsigned line, unsigned desc, unsigned wad, unsigned resident,
    unsigned bypass, unsigned missq, unsigned lowerq, unsigned reserved,
    unsigned resident_valid, unsigned resident_dirty, unsigned resident_pending,
    unsigned bypass_pending, unsigned bypass_ready, unsigned reserved_set_max,
    unsigned descriptor_chain_active, unsigned descriptor_chain_sum,
    unsigned descriptor_chain_max,
    const unsigned long long *descriptor_chain_histogram) {
  ++samples;
  line_sum += line; desc_sum += desc; wad_sum += wad;
  resident_sum += resident; bypass_sum += bypass; missq_sum += missq;
  lowerq_sum += lowerq;
  reserved_sum += reserved; resident_valid_sum += resident_valid;
  resident_dirty_sum += resident_dirty; resident_pending_sum += resident_pending;
  bypass_pending_sum += bypass_pending; bypass_ready_sum += bypass_ready;
  this->descriptor_chain_sum += descriptor_chain_sum;
  descriptor_chain_samples += descriptor_chain_active;
  line_max = std::max(line_max, line); desc_max = std::max(desc_max, desc);
  wad_max = std::max(wad_max, wad); resident_max = std::max(resident_max, resident);
  bypass_max = std::max(bypass_max, bypass); missq_max = std::max(missq_max, missq);
  lowerq_max = std::max(lowerq_max, lowerq);
  reserved_max = std::max(reserved_max, reserved);
  resident_valid_max = std::max(resident_valid_max, resident_valid);
  resident_dirty_max = std::max(resident_dirty_max, resident_dirty);
  resident_pending_max = std::max(resident_pending_max, resident_pending);
  bypass_pending_max = std::max(bypass_pending_max, bypass_pending);
  bypass_ready_max = std::max(bypass_ready_max, bypass_ready);
  this->reserved_set_max = std::max(this->reserved_set_max, reserved_set_max);
  this->descriptor_chain_max =
      std::max(this->descriptor_chain_max, descriptor_chain_max);
  ++line_hist[std::min<unsigned>(line, line_hist.size() - 1)];
  ++desc_hist[std::min<unsigned>(desc, desc_hist.size() - 1)];
  ++wad_hist[std::min<unsigned>(wad, wad_hist.size() - 1)];
  ++resident_hist[std::min<unsigned>(resident, resident_hist.size() - 1)];
  ++bypass_hist[std::min<unsigned>(bypass, bypass_hist.size() - 1)];
  ++reserved_hist[std::min<unsigned>(reserved, reserved_hist.size() - 1)];
  ++resident_valid_hist[std::min<unsigned>(resident_valid, resident_valid_hist.size() - 1)];
  ++resident_dirty_hist[std::min<unsigned>(resident_dirty, resident_dirty_hist.size() - 1)];
  ++resident_pending_hist[std::min<unsigned>(resident_pending, resident_pending_hist.size() - 1)];
  ++bypass_pending_hist[std::min<unsigned>(bypass_pending, bypass_pending_hist.size() - 1)];
  ++bypass_ready_hist[std::min<unsigned>(bypass_ready, bypass_ready_hist.size() - 1)];
  ++reserved_set_hist[std::min<unsigned>(reserved_set_max, reserved_set_hist.size() - 1)];
  for (unsigned i = 0; i < descriptor_chain_hist.size(); ++i)
    descriptor_chain_hist[i] += descriptor_chain_histogram[i];
}

unsigned memory_sub_partition::ep_l2_b0_accum::p95(
    const std::vector<unsigned long long> &hist, unsigned long long n) {
  if (!n) return 0;
  const unsigned long long target = (95 * n + 99) / 100;
  unsigned long long seen = 0;
  for (unsigned i = 0; i < hist.size(); ++i) {
    seen += hist[i];
    if (seen >= target) return i;
  }
  return hist.empty() ? 0 : hist.size() - 1;
}

memory_sub_partition::ep_l2_b0_accum
memory_sub_partition::ep_l2_b0_accum::delta(const ep_l2_b0_accum &start) const {
  ep_l2_b0_accum d(*this);
  d.samples -= start.samples; d.line_sum -= start.line_sum; d.desc_sum -= start.desc_sum;
  d.wad_sum -= start.wad_sum; d.resident_sum -= start.resident_sum;
  d.bypass_sum -= start.bypass_sum; d.missq_sum -= start.missq_sum;
  d.lowerq_sum -= start.lowerq_sum; d.descriptor_block -= start.descriptor_block;
  d.reserved_sum -= start.reserved_sum; d.resident_valid_sum -= start.resident_valid_sum;
  d.resident_dirty_sum -= start.resident_dirty_sum;
  d.resident_pending_sum -= start.resident_pending_sum;
  d.bypass_pending_sum -= start.bypass_pending_sum;
  d.bypass_ready_sum -= start.bypass_ready_sum;
  d.descriptor_chain_sum -= start.descriptor_chain_sum;
  d.descriptor_chain_samples -= start.descriptor_chain_samples;
  d.wad_block -= start.wad_block; d.payload_block -= start.payload_block;
  d.bank_block -= start.bank_block; d.l1_block -= start.l1_block;
  d.lower_block -= start.lower_block;
  d.line_alloc_eligible -= start.line_alloc_eligible;
  d.line_alloc_block -= start.line_alloc_block;
  d.tag_set_all_reserved_block -= start.tag_set_all_reserved_block;
  d.line_mshr_alloc_eligible -= start.line_mshr_alloc_eligible;
  d.line_mshr_full_block -= start.line_mshr_full_block;
  d.descriptor_alloc_eligible -= start.descriptor_alloc_eligible;
  d.descriptor_pool_full_block -= start.descriptor_pool_full_block;
  d.per_address_cap_eligible -= start.per_address_cap_eligible;
  d.per_address_cap_block -= start.per_address_cap_block;
  d.wad_full_events -= start.wad_full_events;
  d.wad_hazard_events -= start.wad_hazard_events;
  d.wad_hazard_wait_cycles -= start.wad_hazard_wait_cycles;
  d.payload_service_port_denial -= start.payload_service_port_denial;
  d.payload_capacity_allocation_denial -= start.payload_capacity_allocation_denial;
  d.missq_full_block -= start.missq_full_block;
  d.l2_to_dram_full_block -= start.l2_to_dram_full_block;
  d.dram_issue_eligible -= start.dram_issue_eligible;
  d.dram_read_issues -= start.dram_read_issues;
  d.dram_write_issues -= start.dram_write_issues;
  d.dram_scheduler_full_block -= start.dram_scheduler_full_block;
  d.dram_returnq_block -= start.dram_returnq_block;
  d.dram_credit_block -= start.dram_credit_block;
  d.dram_return_eligible -= start.dram_return_eligible;
  d.dram_to_l2_full_block -= start.dram_to_l2_full_block;
  d.c7e_tag_way_alloc_need -= start.c7e_tag_way_alloc_need;
  d.c7e_tag_way_alloc_block -= start.c7e_tag_way_alloc_block;
  d.c7e_line_mshr_need -= start.c7e_line_mshr_need;
  d.c7e_descriptor_need -= start.c7e_descriptor_need;
  d.c7e_per_address_cap_check -= start.c7e_per_address_cap_check;
  d.c7e_dram_issue_attempt -= start.c7e_dram_issue_attempt;
  d.c7e_dram_successful_read_issues -= start.c7e_dram_successful_read_issues;
  d.c7e_dram_successful_write_issues -= start.c7e_dram_successful_write_issues;
  d.c7e_dram_successful_read_bytes -= start.c7e_dram_successful_read_bytes;
  d.c7e_dram_successful_write_bytes -= start.c7e_dram_successful_write_bytes;
  d.c7e_dram_scheduler_full_observed -= start.c7e_dram_scheduler_full_observed;
  d.c7e_dram_scheduler_causal_block -= start.c7e_dram_scheduler_causal_block;
  d.c7e_dram_to_l2_return_path_block -= start.c7e_dram_to_l2_return_path_block;
  d.dram_scheduler_occ_sum -= start.dram_scheduler_occ_sum;
  d.dram_scheduler_occ_samples -= start.dram_scheduler_occ_samples;
  for (unsigned i = 0; i < d.line_hist.size(); ++i) d.line_hist[i] -= start.line_hist[i];
  for (unsigned i = 0; i < d.desc_hist.size(); ++i) d.desc_hist[i] -= start.desc_hist[i];
  for (unsigned i = 0; i < d.wad_hist.size(); ++i) d.wad_hist[i] -= start.wad_hist[i];
  for (unsigned i = 0; i < d.resident_hist.size(); ++i) d.resident_hist[i] -= start.resident_hist[i];
  for (unsigned i = 0; i < d.bypass_hist.size(); ++i) d.bypass_hist[i] -= start.bypass_hist[i];
  for (unsigned i = 0; i < d.reserved_hist.size(); ++i) d.reserved_hist[i] -= start.reserved_hist[i];
  for (unsigned i = 0; i < d.resident_valid_hist.size(); ++i) d.resident_valid_hist[i] -= start.resident_valid_hist[i];
  for (unsigned i = 0; i < d.resident_dirty_hist.size(); ++i) d.resident_dirty_hist[i] -= start.resident_dirty_hist[i];
  for (unsigned i = 0; i < d.resident_pending_hist.size(); ++i) d.resident_pending_hist[i] -= start.resident_pending_hist[i];
  for (unsigned i = 0; i < d.bypass_pending_hist.size(); ++i) d.bypass_pending_hist[i] -= start.bypass_pending_hist[i];
  for (unsigned i = 0; i < d.bypass_ready_hist.size(); ++i) d.bypass_ready_hist[i] -= start.bypass_ready_hist[i];
  for (unsigned i = 0; i < d.reserved_set_hist.size(); ++i) d.reserved_set_hist[i] -= start.reserved_set_hist[i];
  for (unsigned i = 0; i < d.descriptor_chain_hist.size(); ++i)
    d.descriptor_chain_hist[i] -= start.descriptor_chain_hist[i];
  for (unsigned i = 0; i < d.dram_scheduler_occ_hist.size(); ++i)
    d.dram_scheduler_occ_hist[i] -= start.dram_scheduler_occ_hist[i];
  // Maxima are interval values for kernel snapshots, not cumulative maxima
  // inherited from the application accumulator.
  const std::vector<unsigned long long> *hists[] = {
      &d.line_hist, &d.desc_hist, &d.wad_hist, &d.resident_hist,
      &d.bypass_hist, &d.reserved_hist, &d.resident_valid_hist,
      &d.resident_dirty_hist, &d.resident_pending_hist,
      &d.bypass_pending_hist, &d.bypass_ready_hist, &d.reserved_set_hist,
      &d.descriptor_chain_hist,
      &d.dram_scheduler_occ_hist};
  unsigned *maxes[] = {&d.line_max, &d.desc_max, &d.wad_max, &d.resident_max,
                       &d.bypass_max, &d.reserved_max, &d.resident_valid_max,
                       &d.resident_dirty_max, &d.resident_pending_max,
                       &d.bypass_pending_max, &d.bypass_ready_max,
                       &d.reserved_set_max, &d.descriptor_chain_max,
                       &d.dram_scheduler_occ_max};
  for (unsigned h = 0; h < sizeof(hists) / sizeof(hists[0]); ++h) {
    *maxes[h] = 0;
    for (unsigned i = 0; i < hists[h]->size(); ++i)
      if ((*hists[h])[i]) *maxes[h] = i;
  }
  return d;
}

void memory_sub_partition::ep_l2_b0_sample(unsigned long long cycle) {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  unsigned reserved = 0, dirty = 0, valid = 0, reserved_set_max = 0;
  m_L2cache->l2_char_storage_snapshot_compact(
      reserved, dirty, valid, reserved_set_max);
  unsigned descriptor_chain_active = 0, descriptor_chain_sum = 0,
           descriptor_chain_max = 0;
  unsigned long long descriptor_chain_hist[33];
  m_L2cache->descriptor_chain_snapshot(
      descriptor_chain_active, descriptor_chain_sum, descriptor_chain_max,
      descriptor_chain_hist, 33);
  m_ep_l2_b0_accum.sample(
      m_L2cache->mshr_entries_used(), m_L2cache->ep_l2_descriptor_count_used(),
      m_L2cache->ep_l2_wad_occupancy(),
      m_L2cache->ep_l2_resident_payload_occupancy(),
      m_L2cache->ep_l2_bypass_payload_occupancy(),
      m_L2cache->miss_queue_occupancy(), m_L2_dram_queue->get_n_element(),
      reserved, m_L2cache->ep_l2_resident_valid(),
      m_L2cache->ep_l2_resident_dirty(), m_L2cache->ep_l2_resident_pending(),
      m_L2cache->ep_l2_bypass_pending(), m_L2cache->ep_l2_bypass_ready(),
      reserved_set_max, descriptor_chain_active, descriptor_chain_sum,
      descriptor_chain_max, descriptor_chain_hist);
  if (!m_ep_l2_b0_window_started) {
    m_ep_l2_b0_window_start = m_ep_l2_b0_accum;
    m_ep_l2_b0_window_bank_start = ep_l2_b0_bank_snapshot();
    m_ep_l2_b0_window_start_cycle = cycle;
    m_ep_l2_b0_window_started = true;
    return;
  }
  if (cycle - m_ep_l2_b0_window_start_cycle < 5000) return;
  const ep_l2_b0_accum window =
      m_ep_l2_b0_accum.delta(m_ep_l2_b0_window_start);
  const ep_l2_b0_bank_accum bank =
      ep_l2_b0_bank_snapshot().delta(m_ep_l2_b0_window_bank_start);
  const unsigned long long n = window.samples;
  fprintf(stdout,
          "EPL2B0V1|scope=window|interval=5000_cycle|slice=%u|"
          "start_cycle=%llu|completion_cycle=%llu|samples=%llu|"
          "line_mshr_avg=%llu|descriptor_avg=%llu|wad_avg=%llu|"
          "resident_payload_avg=%llu|missq_avg=%llu|lowerq_avg=%llu|"
          "bank_logical_ops=%llu|bank_true_conflict_ops=%llu|"
          "bank_wait_cycles=%llu\n",
          m_id, m_ep_l2_b0_window_start_cycle, cycle, n,
          n ? window.line_sum / n : 0, n ? window.desc_sum / n : 0,
          n ? window.wad_sum / n : 0, n ? window.resident_sum / n : 0,
          n ? window.missq_sum / n : 0, n ? window.lowerq_sum / n : 0,
          bank.logical, bank.true_ops, bank.wait);
  m_ep_l2_b0_window_start = m_ep_l2_b0_accum;
  m_ep_l2_b0_window_bank_start = ep_l2_b0_bank_snapshot();
  m_ep_l2_b0_window_start_cycle = cycle;
}

memory_sub_partition::ep_l2_b0_bank_accum
memory_sub_partition::ep_l2_b0_bank_accum::delta(
    const ep_l2_b0_bank_accum &start) const {
  ep_l2_b0_bank_accum d(*this);
  d.requests -= start.requests; d.grants -= start.grants;
  d.conflicts -= start.conflicts; d.logical -= start.logical;
  d.attempts -= start.attempts; d.retries -= start.retries;
  d.true_ops -= start.true_ops; d.true_events -= start.true_events;
  d.wait -= start.wait; d.resident_hit_read -= start.resident_hit_read;
  d.resident_write -= start.resident_write; d.fill_write -= start.fill_write;
  d.wb_readout -= start.wb_readout; d.bypass_fill -= start.bypass_fill;
  d.bypass_read -= start.bypass_read;
  for (unsigned b = 0; b < 4; ++b) {
    d.logical_by_bank[b] -= start.logical_by_bank[b];
    d.grants_by_bank[b] -= start.grants_by_bank[b];
    d.true_ops_by_bank[b] -= start.true_ops_by_bank[b];
    d.true_events_by_bank[b] -= start.true_events_by_bank[b];
    d.wait_by_bank[b] -= start.wait_by_bank[b];
  }
  return d;
}

memory_sub_partition::ep_l2_b0_bank_accum
memory_sub_partition::ep_l2_b0_bank_snapshot() const {
  ep_l2_b0_bank_accum s;
  s.requests = m_L2cache->ep_l2_payload_bank_requests();
  s.grants = m_L2cache->ep_l2_payload_bank_grants();
  s.conflicts = m_L2cache->ep_l2_payload_bank_conflicts();
  s.logical = m_L2cache->ep_l2_payload_bank_logical_ops();
  s.attempts = m_L2cache->ep_l2_payload_bank_attempts();
  s.retries = m_L2cache->ep_l2_payload_bank_retry_attempts();
  s.true_ops = m_L2cache->ep_l2_payload_bank_true_conflict_ops();
  s.true_events = m_L2cache->ep_l2_payload_bank_true_conflict_events();
  s.wait = m_L2cache->ep_l2_payload_bank_wait_cycles();
  s.resident_hit_read = m_L2cache->ep_l2_payload_bank_resident_hit_read();
  s.resident_write = m_L2cache->ep_l2_payload_bank_resident_write();
  s.fill_write = m_L2cache->ep_l2_payload_bank_fill_write();
  s.wb_readout = m_L2cache->ep_l2_payload_bank_wb_readout();
  s.bypass_fill = m_L2cache->ep_l2_payload_bank_bypass_fill();
  s.bypass_read = m_L2cache->ep_l2_payload_bank_bypass_read();
  for (unsigned b = 0; b < 4; ++b) {
    s.logical_by_bank[b] = m_L2cache->ep_l2_payload_bank_logical_ops(b);
    s.grants_by_bank[b] = m_L2cache->ep_l2_payload_bank_grants(b);
    s.true_ops_by_bank[b] = m_L2cache->ep_l2_payload_bank_true_conflict_ops(b);
    s.true_events_by_bank[b] = m_L2cache->ep_l2_payload_bank_true_conflict_events(b);
    s.wait_by_bank[b] = m_L2cache->ep_l2_payload_bank_wait_cycles(b);
  }
  return s;
}

void memory_sub_partition::begin_ep_l2_b0_kernel(unsigned long long uid) {
  const bool overlap = !m_ep_l2_b0_kernel_start.empty();
  if (overlap)
    for (std::map<unsigned long long, bool>::iterator it =
             m_ep_l2_b0_kernel_overlap.begin();
         it != m_ep_l2_b0_kernel_overlap.end(); ++it)
      it->second = true;
  m_ep_l2_b0_kernel_start[uid] = m_ep_l2_b0_accum;
  m_ep_l2_b0_kernel_bank_start[uid] = ep_l2_b0_bank_snapshot();
  m_ep_l2_b0_kernel_start_cycle[uid] = m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle;
  m_ep_l2_b0_kernel_overlap[uid] = overlap;
}

void memory_sub_partition::end_ep_l2_b0_kernel(FILE *fp,
                                                unsigned long long uid) {
  print_ep_l2_b0_snapshot(fp, uid);
  m_ep_l2_b0_kernel_start.erase(uid);
  m_ep_l2_b0_kernel_bank_start.erase(uid);
  m_ep_l2_b0_kernel_start_cycle.erase(uid);
  m_ep_l2_b0_kernel_overlap.erase(uid);
}

void memory_sub_partition::print_ep_l2_b0_snapshot(FILE *fp,
                                                    unsigned long long uid) const {
  if (!m_config->m_L2_config.ep_l2_b0_stats_enabled()) return;
  ep_l2_b0_accum stats = m_ep_l2_b0_accum;
  if (uid != (unsigned long long)-1) {
    std::map<unsigned long long, ep_l2_b0_accum>::const_iterator start =
        m_ep_l2_b0_kernel_start.find(uid);
    if (start != m_ep_l2_b0_kernel_start.end()) stats = stats.delta(start->second);
  }
  const unsigned long long n = stats.samples;
  unsigned long long start_cycle = 0;
  bool overlap = false;
  if (uid != (unsigned long long)-1) {
    std::map<unsigned long long, unsigned long long>::const_iterator start =
        m_ep_l2_b0_kernel_start_cycle.find(uid);
    if (start != m_ep_l2_b0_kernel_start_cycle.end()) start_cycle = start->second;
    std::map<unsigned long long, bool>::const_iterator overlap_it =
        m_ep_l2_b0_kernel_overlap.find(uid);
    if (overlap_it != m_ep_l2_b0_kernel_overlap.end()) overlap = overlap_it->second;
  }
  ep_l2_b0_bank_accum bank = ep_l2_b0_bank_snapshot();
  if (uid != (unsigned long long)-1) {
    std::map<unsigned long long, ep_l2_b0_bank_accum>::const_iterator start =
        m_ep_l2_b0_kernel_bank_start.find(uid);
    if (start != m_ep_l2_b0_kernel_bank_start.end()) bank = bank.delta(start->second);
  }
  fprintf(fp,
          "EPL2B0V1|scope=%s|interval=%s|slice=%u|kernel_uid=%llu|start_cycle=%llu|completion_cycle=%llu|overlap_detected=%u|samples=%llu|"
          "line_mshr_avg=%llu|line_mshr_p95=%u|line_mshr_max=%u|"
          "descriptor_avg=%llu|descriptor_p95=%u|descriptor_max=%u|"
          "wad_avg=%llu|wad_p95=%u|wad_max=%u|resident_payload_avg=%llu|"
          "resident_payload_p95=%u|resident_payload_max=%u|bypass_payload_avg=%llu|"
          "bypass_payload_p95=%u|bypass_payload_max=%u|missq_avg=%llu|missq_max=%u|"
          "lowerq_avg=%llu|lowerq_max=%u|block_descriptor=%llu|block_wad=%llu|"
          "block_payload=%llu|block_bank=%llu|block_l1=%llu|block_lower=%llu|"
          "bank_requests=%llu|bank_grants=%llu|bank_conflicts=%llu|"
          "bank_logical_ops=%llu|bank_attempts=%llu|bank_retry_attempts=%llu|"
          "bank_true_conflict_ops=%llu|bank_true_conflict_events=%llu|"
          "bank_wait_cycles=%llu|"
          "c7d_line_alloc_eligible=%llu|c7d_line_alloc_block=%llu|"
          "c7d_tag_set_all_reserved_block=%llu|"
          "c7d_line_mshr_alloc_eligible=%llu|c7d_line_mshr_full_block=%llu|"
          "c7d_descriptor_alloc_eligible=%llu|c7d_descriptor_pool_full_block=%llu|"
          "c7d_per_address_cap_eligible=%llu|c7d_per_address_cap_block=%llu|"
          "c7d_descriptor_chain_depth_avg=%llu|c7d_descriptor_chain_depth_p95=%u|"
          "c7d_descriptor_chain_depth_max=%u|"
          "c7d_wad_full_events=%llu|c7d_wad_hazard_events=%llu|c7d_wad_hazard_wait_cycles=%llu|"
          "c7d_wad_lifetime_avg=%llu|c7d_wad_lifetime_p95=%llu|c7d_wad_lifetime_max=%llu|"
          "c7d_reserved_avg=%llu|c7d_reserved_p95=%u|c7d_reserved_max=%u|c7d_reserved_set_max=%u|"
          "c7d_resident_valid_avg=%llu|c7d_resident_valid_p95=%u|c7d_resident_valid_max=%u|"
          "c7d_resident_dirty_avg=%llu|c7d_resident_dirty_p95=%u|c7d_resident_dirty_max=%u|"
          "c7d_resident_pending_sector_avg=%llu|c7d_resident_pending_sector_p95=%u|c7d_resident_pending_sector_max=%u|"
          "c7d_bypass_pending_avg=%llu|c7d_bypass_pending_p95=%u|c7d_bypass_pending_max=%u|"
          "c7d_bypass_ready_avg=%llu|c7d_bypass_ready_p95=%u|c7d_bypass_ready_max=%u|"
          "c7d_payload_service_port_denial=%llu|c7d_payload_capacity_allocation_denial=%llu|"
          "c7d_missq_full_block=%llu|c7d_l2_to_dram_full_block=%llu|"
          "c7d_dram_issue_eligible=%llu|c7d_dram_read_issues=%llu|c7d_dram_write_issues=%llu|"
          "c7d_dram_scheduler_full_block=%llu|c7d_dram_returnq_block=%llu|"
          "c7d_dram_credit_block=%llu|c7d_dram_return_eligible=%llu|"
          "c7d_dram_to_l2_full_block=%llu|c7d_dram_scheduler_occ_avg=%llu|"
          "c7d_dram_scheduler_occ_max=%u|"
          "c7d_bank0_logical_ops=%llu|c7d_bank1_logical_ops=%llu|c7d_bank2_logical_ops=%llu|c7d_bank3_logical_ops=%llu|"
          "c7d_bank0_grants=%llu|c7d_bank1_grants=%llu|c7d_bank2_grants=%llu|c7d_bank3_grants=%llu|"
          "c7d_bank0_true_conflict_ops=%llu|c7d_bank1_true_conflict_ops=%llu|c7d_bank2_true_conflict_ops=%llu|c7d_bank3_true_conflict_ops=%llu|"
          "c7d_bank0_wait_cycles=%llu|c7d_bank1_wait_cycles=%llu|c7d_bank2_wait_cycles=%llu|c7d_bank3_wait_cycles=%llu|"
          "c7d_bank_resident_hit_read=%llu|c7d_bank_resident_write=%llu|c7d_bank_fill_write=%llu|c7d_bank_wb_readout=%llu|c7d_bank_bypass_fill=%llu|c7d_bank_bypass_read=%llu|"
          "c7e_tag_way_alloc_need=%llu|c7e_tag_way_alloc_block=%llu|"
          "c7e_line_mshr_need=%llu|c7e_descriptor_need=%llu|c7e_per_address_cap_check=%llu|"
          "c7e_dram_issue_attempt=%llu|c7e_dram_successful_read_issues=%llu|c7e_dram_successful_write_issues=%llu|"
          "c7e_dram_successful_read_bytes=%llu|c7e_dram_successful_write_bytes=%llu|"
          "c7e_dram_scheduler_full_observed=%llu|c7e_dram_scheduler_causal_block=%llu|"
          "c7e_dram_to_l2_return_path_block=%llu|c7e_wad_lifetime_kernel_available=%u\n",
          uid == (unsigned long long)-1 ? "application" : "kernel",
          uid == (unsigned long long)-1 ? "application_cumulative" : "kernel_shared_delta",
          m_id, uid, start_cycle, m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle,
          overlap ? 1 : 0, n, n ? stats.line_sum / n : 0,
          ep_l2_b0_accum::p95(stats.line_hist, n), stats.line_max,
          n ? stats.desc_sum / n : 0, ep_l2_b0_accum::p95(stats.desc_hist, n), stats.desc_max,
          n ? stats.wad_sum / n : 0, ep_l2_b0_accum::p95(stats.wad_hist, n), stats.wad_max,
          n ? stats.resident_sum / n : 0, ep_l2_b0_accum::p95(stats.resident_hist, n), stats.resident_max,
          n ? stats.bypass_sum / n : 0, ep_l2_b0_accum::p95(stats.bypass_hist, n), stats.bypass_max,
          n ? stats.missq_sum / n : 0, stats.missq_max, n ? stats.lowerq_sum / n : 0,
          stats.lowerq_max, stats.descriptor_block, stats.wad_block,
          stats.payload_block, stats.bank_block, stats.l1_block, stats.lower_block,
          bank.requests, bank.grants, bank.conflicts, bank.logical, bank.attempts,
          bank.retries, bank.true_ops, bank.true_events, bank.wait,
          stats.line_alloc_eligible, stats.line_alloc_block,
          stats.tag_set_all_reserved_block, stats.line_mshr_alloc_eligible,
          stats.line_mshr_full_block, stats.descriptor_alloc_eligible,
          stats.descriptor_pool_full_block, stats.per_address_cap_eligible,
          stats.per_address_cap_block,
          stats.descriptor_chain_samples ?
              stats.descriptor_chain_sum / stats.descriptor_chain_samples : 0,
          ep_l2_b0_accum::p95(stats.descriptor_chain_hist,
                               stats.descriptor_chain_samples),
          stats.descriptor_chain_max, stats.wad_full_events,
          stats.wad_hazard_events, stats.wad_hazard_wait_cycles,
          m_L2cache->ep_l2_wad_lifetime_count() ?
              m_L2cache->ep_l2_wad_lifetime_sum() / m_L2cache->ep_l2_wad_lifetime_count() : 0,
          m_L2cache->ep_l2_wad_lifetime_p95(), m_L2cache->ep_l2_wad_lifetime_max(),
          n ? stats.reserved_sum / n : 0, ep_l2_b0_accum::p95(stats.reserved_hist, n), stats.reserved_max,
          stats.reserved_set_max,
          n ? stats.resident_valid_sum / n : 0, ep_l2_b0_accum::p95(stats.resident_valid_hist, n), stats.resident_valid_max,
          n ? stats.resident_dirty_sum / n : 0, ep_l2_b0_accum::p95(stats.resident_dirty_hist, n), stats.resident_dirty_max,
          n ? stats.resident_pending_sum / n : 0, ep_l2_b0_accum::p95(stats.resident_pending_hist, n), stats.resident_pending_max,
          n ? stats.bypass_pending_sum / n : 0, ep_l2_b0_accum::p95(stats.bypass_pending_hist, n), stats.bypass_pending_max,
          n ? stats.bypass_ready_sum / n : 0, ep_l2_b0_accum::p95(stats.bypass_ready_hist, n), stats.bypass_ready_max,
          stats.payload_service_port_denial, stats.payload_capacity_allocation_denial,
          stats.missq_full_block, stats.l2_to_dram_full_block,
          stats.dram_issue_eligible, stats.dram_read_issues, stats.dram_write_issues,
          stats.dram_scheduler_full_block, stats.dram_returnq_block,
          stats.dram_credit_block, stats.dram_return_eligible,
          stats.dram_to_l2_full_block,
          stats.dram_scheduler_occ_samples ?
              stats.dram_scheduler_occ_sum / stats.dram_scheduler_occ_samples : 0,
          stats.dram_scheduler_occ_max,
          bank.logical_by_bank[0], bank.logical_by_bank[1], bank.logical_by_bank[2], bank.logical_by_bank[3],
          bank.grants_by_bank[0], bank.grants_by_bank[1], bank.grants_by_bank[2], bank.grants_by_bank[3],
          bank.true_ops_by_bank[0], bank.true_ops_by_bank[1], bank.true_ops_by_bank[2], bank.true_ops_by_bank[3],
          bank.wait_by_bank[0], bank.wait_by_bank[1], bank.wait_by_bank[2], bank.wait_by_bank[3],
          bank.resident_hit_read, bank.resident_write, bank.fill_write,
          bank.wb_readout, bank.bypass_fill, bank.bypass_read,
          stats.c7e_tag_way_alloc_need, stats.c7e_tag_way_alloc_block,
          stats.c7e_line_mshr_need, stats.c7e_descriptor_need,
          stats.c7e_per_address_cap_check, stats.c7e_dram_issue_attempt,
          stats.c7e_dram_successful_read_issues,
          stats.c7e_dram_successful_write_issues,
          stats.c7e_dram_successful_read_bytes,
          stats.c7e_dram_successful_write_bytes,
          stats.c7e_dram_scheduler_full_observed,
          stats.c7e_dram_scheduler_causal_block,
          stats.c7e_dram_to_l2_return_path_block,
          uid == (unsigned long long)-1 ? 1 : 0);
  fprintf(fp,
          "EPL2B0V1|INVARIANT|slice=%u|kernel_uid=%llu|line_mshr_used=%u|"
          "line_mshr_capacity=%u|descriptor_used=%u|descriptor_free=%u|"
          "descriptor_capacity=%u|wad_live=%u|wad_capacity=%u|resident_live=%u|"
          "resident_capacity=1024|resident_pending=%u|bypass_live=%u|bypass_capacity=128|bank_pending=%u|"
          "resident_tag_payload_consistent=%u|payload_double_owner=%u|terminal_clean=%u\n",
          m_id, uid, m_L2cache->mshr_entries_used(),
          m_L2cache->mshr_entry_capacity(), m_L2cache->ep_l2_descriptor_count_used(),
          m_L2cache->ep_l2_descriptor_pool_capacity() - m_L2cache->ep_l2_descriptor_count_used(),
          m_L2cache->ep_l2_descriptor_pool_capacity(),
          m_L2cache->ep_l2_wad_occupancy(), m_L2cache->ep_l2_wad_capacity(),
          m_L2cache->ep_l2_resident_payload_occupancy(),
          m_L2cache->ep_l2_resident_pending(),
          m_L2cache->ep_l2_bypass_payload_occupancy(),
          m_L2cache->ep_l2_payload_pending_operations(),
          m_L2cache->ep_l2_payload_ownership_consistent() ? 1 : 0,
          m_L2cache->ep_l2_payload_ownership_consistent() ? 0 : 1,
          (m_L2cache->ep_l2_descriptor_count_used() == 0 &&
           m_L2cache->ep_l2_wad_occupancy() == 0 &&
           m_L2cache->ep_l2_resident_pending() == 0 &&
           m_L2cache->ep_l2_bypass_payload_occupancy() == 0 &&
           m_L2cache->ep_l2_payload_pending_operations() == 0) ? 1 : 0);
}

bool memory_sub_partition::l2_char_no_resource_leak() const {
  return m_request_tracker.empty() && m_icnt_L2_queue->empty() &&
         m_L2_dram_queue->empty() && m_dram_L2_queue->empty() &&
         m_L2_icnt_queue->empty() && m_rop.empty() &&
         m_l2_block_state.empty() && m_L2cache->l2_char_no_pending_resources() &&
         m_L2cache->ep_l2_resident_pending() == 0 &&
         m_L2cache->ep_l2_payload_pending_operations() == 0;
}

void memory_stats_t::visualizer_print(gzFile visualizer_file) {
  gzprintf(visualizer_file, "Ltwowritemiss: %d\n", L2_write_miss);
  gzprintf(visualizer_file, "Ltwowritehit: %d\n", L2_write_hit);
  gzprintf(visualizer_file, "Ltworeadmiss: %d\n", L2_read_miss);
  gzprintf(visualizer_file, "Ltworeadhit: %d\n", L2_read_hit);
  clear_L2_stats_pw();

  if (num_mfs)
    gzprintf(visualizer_file, "averagemflatency: %lld\n",
             mf_total_lat / num_mfs);
}

void memory_stats_t::clear_L2_stats_pw() {
  L2_write_miss = 0;
  L2_write_hit = 0;
  L2_read_miss = 0;
  L2_read_hit = 0;
}

void gpgpu_sim::print_dram_stats(FILE *fout) const {
  unsigned cmd = 0;
  unsigned activity = 0;
  unsigned nop = 0;
  unsigned act = 0;
  unsigned pre = 0;
  unsigned rd = 0;
  unsigned wr = 0;
  unsigned wr_WB = 0;
  unsigned req = 0;
  unsigned tot_cmd = 0;
  unsigned tot_nop = 0;
  unsigned tot_act = 0;
  unsigned tot_pre = 0;
  unsigned tot_rd = 0;
  unsigned tot_wr = 0;
  unsigned tot_req = 0;

  for (unsigned i = 0; i < m_memory_config->m_n_mem; i++) {
    m_memory_partition_unit[i]->set_dram_power_stats(cmd, activity, nop, act,
                                                     pre, rd, wr, wr_WB, req);
    tot_cmd += cmd;
    tot_nop += nop;
    tot_act += act;
    tot_pre += pre;
    tot_rd += rd;
    tot_wr += wr + wr_WB;
    tot_req += req;
  }
  fprintf(fout, "gpgpu_n_dram_reads = %d\n", tot_rd);
  fprintf(fout, "gpgpu_n_dram_writes = %d\n", tot_wr);
  fprintf(fout, "gpgpu_n_dram_activate = %d\n", tot_act);
  fprintf(fout, "gpgpu_n_dram_commands = %d\n", tot_cmd);
  fprintf(fout, "gpgpu_n_dram_noops = %d\n", tot_nop);
  fprintf(fout, "gpgpu_n_dram_precharges = %d\n", tot_pre);
  fprintf(fout, "gpgpu_n_dram_requests = %d\n", tot_req);
}

unsigned memory_sub_partition::flushL2() {
  if (!m_config->m_L2_config.disabled()) {
    m_L2cache->flush();
  }
  return 0;  // TODO: write the flushed data to the main memory
}

unsigned memory_sub_partition::invalidateL2() {
  if (!m_config->m_L2_config.disabled()) {
    m_L2cache->invalidate();
  }
  return 0;
}

bool memory_sub_partition::busy() const { return !m_request_tracker.empty(); }

std::vector<mem_fetch *>
memory_sub_partition::breakdown_request_to_sector_requests(mem_fetch *mf) {
  std::vector<mem_fetch *> result;
  mem_access_sector_mask_t sector_mask = mf->get_access_sector_mask();
  if (mf->get_data_size() == SECTOR_SIZE &&
      mf->get_access_sector_mask().count() == 1) {
    result.push_back(mf);
  } else if (mf->get_data_size() == MAX_MEMORY_ACCESS_SIZE) {
    // break down every sector
    mem_access_byte_mask_t mask;
    for (unsigned i = 0; i < SECTOR_CHUNCK_SIZE; i++) {
      for (unsigned k = i * SECTOR_SIZE; k < (i + 1) * SECTOR_SIZE; k++) {
        mask.set(k);
      }
      mem_fetch *n_mf = m_mf_allocator->alloc(
          mf->get_addr() + SECTOR_SIZE * i, mf->get_access_type(),
          mf->get_access_warp_mask(), mf->get_access_byte_mask() & mask,
          std::bitset<SECTOR_CHUNCK_SIZE>().set(i), SECTOR_SIZE, mf->is_write(),
          m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, mf->get_wid(),
          mf->get_sid(), mf->get_tpc(), mf, mf->get_streamID());

      result.push_back(n_mf);
    }
    // This is for constant cache
  } else if (mf->get_data_size() == 64 &&
             (mf->get_access_sector_mask().all() ||
              mf->get_access_sector_mask().none())) {
    unsigned start;
    if (mf->get_addr() % MAX_MEMORY_ACCESS_SIZE == 0)
      start = 0;
    else
      start = 2;
    mem_access_byte_mask_t mask;
    for (unsigned i = start; i < start + 2; i++) {
      for (unsigned k = i * SECTOR_SIZE; k < (i + 1) * SECTOR_SIZE; k++) {
        mask.set(k);
      }
      mem_fetch *n_mf = m_mf_allocator->alloc(
          mf->get_addr(), mf->get_access_type(), mf->get_access_warp_mask(),
          mf->get_access_byte_mask() & mask,
          std::bitset<SECTOR_CHUNCK_SIZE>().set(i), SECTOR_SIZE, mf->is_write(),
          m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, mf->get_wid(),
          mf->get_sid(), mf->get_tpc(), mf, mf->get_streamID());

      result.push_back(n_mf);
    }
  } else {
    for (unsigned i = 0; i < SECTOR_CHUNCK_SIZE; i++) {
      if (sector_mask.test(i)) {
        mem_access_byte_mask_t mask;
        for (unsigned k = i * SECTOR_SIZE; k < (i + 1) * SECTOR_SIZE; k++) {
          mask.set(k);
        }
        mem_fetch *n_mf = m_mf_allocator->alloc(
            mf->get_addr() + SECTOR_SIZE * i, mf->get_access_type(),
            mf->get_access_warp_mask(), mf->get_access_byte_mask() & mask,
            std::bitset<SECTOR_CHUNCK_SIZE>().set(i), SECTOR_SIZE,
            mf->is_write(), m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle,
            mf->get_wid(), mf->get_sid(), mf->get_tpc(), mf,
            mf->get_streamID());

        result.push_back(n_mf);
      }
    }
  }
  if (result.size() == 0) assert(0 && "no mf sent");
  return result;
}

void memory_sub_partition::push(mem_fetch *m_req, unsigned long long cycle) {
  if (m_req) {
    m_stats->memlatstat_icnt2mem_pop(m_req);
    std::vector<mem_fetch *> reqs;
    if (m_config->m_L2_config.m_cache_type == SECTOR)
      reqs = breakdown_request_to_sector_requests(m_req);
    else
      reqs.push_back(m_req);

    for (unsigned i = 0; i < reqs.size(); ++i) {
      mem_fetch *req = reqs[i];
      m_request_tracker.insert(req);
      if (req->istexture()) {
        m_icnt_L2_queue->push(req);
        req->set_status(IN_PARTITION_ICNT_TO_L2_QUEUE,
                        m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      } else {
        rop_delay_t r;
        r.req = req;
        r.ready_cycle = cycle + m_config->rop_latency;
        m_rop.push(r);
        req->set_status(IN_PARTITION_ROP_DELAY,
                        m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
      }
    }
  }
}

mem_fetch *memory_sub_partition::pop() {
  mem_fetch *mf = m_L2_icnt_queue->pop();
  m_request_tracker.erase(mf);
  if (mf && mf->isatomic()) mf->do_atomic();
  if (mf && (mf->get_access_type() == L2_WRBK_ACC ||
             mf->get_access_type() == L1_WRBK_ACC)) {
    delete mf;
    mf = NULL;
  }
  return mf;
}

mem_fetch *memory_sub_partition::top() {
  mem_fetch *mf = m_L2_icnt_queue->top();
  if (mf && (mf->get_access_type() == L2_WRBK_ACC ||
             mf->get_access_type() == L1_WRBK_ACC)) {
    m_L2_icnt_queue->pop();
    m_request_tracker.erase(mf);
    delete mf;
    mf = NULL;
  }
  return mf;
}

void memory_sub_partition::set_done(mem_fetch *mf) {
  if (mf->get_access_type() == L2_WRBK_ACC)
    m_L2cache->ep_l2_wad_complete(
        mf->get_addr(), m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
  m_request_tracker.erase(mf);
}

void memory_sub_partition::accumulate_L2cache_stats(
    class cache_stats &l2_stats) const {
  if (!m_config->m_L2_config.disabled()) {
    l2_stats += m_L2cache->get_stats();
  }
}

void memory_sub_partition::get_L2cache_sub_stats(
    struct cache_sub_stats &css) const {
  if (!m_config->m_L2_config.disabled()) {
    m_L2cache->get_sub_stats(css);
  }
}

void memory_sub_partition::get_L2cache_sub_stats_pw(
    struct cache_sub_stats_pw &css) const {
  if (!m_config->m_L2_config.disabled()) {
    m_L2cache->get_sub_stats_pw(css);
  }
}

void memory_sub_partition::clear_L2cache_stats_pw() {
  if (!m_config->m_L2_config.disabled()) {
    m_L2cache->clear_pw();
  }
}

void memory_sub_partition::visualizer_print(gzFile visualizer_file) {
  // Support for L2 AerialVision stats
  // Per-sub-partition stats would be trivial to extend from this
  cache_sub_stats_pw temp_sub_stats;
  get_L2cache_sub_stats_pw(temp_sub_stats);

  m_stats->L2_read_miss += temp_sub_stats.read_misses;
  m_stats->L2_write_miss += temp_sub_stats.write_misses;
  m_stats->L2_read_hit += temp_sub_stats.read_hits;
  m_stats->L2_write_hit += temp_sub_stats.write_hits;

  clear_L2cache_stats_pw();
}
