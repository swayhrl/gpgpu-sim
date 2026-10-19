#ifndef EP_L2_MECHANISM_OBSERVER_H
#define EP_L2_MECHANISM_OBSERVER_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include "ep-l2-mechanism-interface.h"

namespace ep_l2 {
namespace mechanism_v1 {
// A single FIFO frontend head is observed per slice. No per-access history,
// shadow address directory, simulated capacity or controller decision lives
// here. All durations use physical GPU time, never the host memcpy offset.
class Observer {
 public:
  typedef unsigned long long count;
  static const unsigned k_bins = 65;
  struct Wait {
    count requests = 0, observations = 0, resolved = 0, sum = 0, maximum = 0;
    count unknown_while_unresolved = 0;
    std::array<count, k_bins> histogram{};
  };
  bool enabled = false;
  std::array<Wait, WAIT_REASONS> waits{};
  Wait any_to_admission;
  count head_changes_without_admission = 0;
  count unclassified_refusal_observations = 0;
  count sampled_cycles = 0, wb_occupancy_sum = 0, wad_occupancy_sum = 0;
  unsigned wb_peak = 0, wad_peak = 0, descriptor_peak = 0;
  unsigned physical_resident_peak = 0, bound_tag_peak = 0;
  unsigned pending_landing_peak = 0;
  count lower_reads = 0, lower_read_bytes = 0, lower_writes = 0;
  count lower_write_bytes = 0, lower_wb_bytes = 0;

  static unsigned bin(count duration) {
    unsigned b = 0;
    for (; duration && b < k_bins - 1; ++b) duration >>= 1;
    return b;  // 0; 1; 2..3; 4..7; ...; saturated 64-bit final bin.
  }
  static void resolve(Wait &w, count duration) {
    ++w.resolved; w.sum += duration;
    w.maximum = std::max(w.maximum, duration); ++w.histogram[bin(duration)];
  }
  void observe(unsigned uid, count cycle, count blocked, bool admitted,
               count known = (1ULL << WAIT_REASONS) - 1) {
    if (!enabled) return;
    if (!active_ || uid_ != uid) {
      if (active_) ++head_changes_without_admission;
      active_ = true; uid_ = uid; seen_ = unresolved_ = 0;
      any_blocked_ = false;
    }
    if (admitted) { blocked = 0; known = (1ULL << WAIT_REASONS) - 1; }
    assert((blocked & ~known) == 0);
    if (!admitted && !blocked) ++unclassified_refusal_observations;
    if (blocked && !any_blocked_) {
      any_blocked_ = true; first_any_ = cycle; ++any_to_admission.requests;
    }
    if (blocked) ++any_to_admission.observations;
    for (unsigned r = 0; r < WAIT_REASONS; ++r) {
      const count bit = 1ULL << r;
      if (!(known & bit)) {
        if (unresolved_ & bit) ++waits[r].unknown_while_unresolved;
        continue;  // An earlier native guard can hide this resource check.
      }
      if (blocked & bit) {
        ++waits[r].observations;
        if (!(seen_ & bit)) {
          seen_ |= bit; unresolved_ |= bit; first_[r] = cycle;
          ++waits[r].requests;
        }
      } else if (unresolved_ & bit) {
        assert(cycle >= first_[r]);
        resolve(waits[r], cycle - first_[r]); unresolved_ &= ~bit;
      }
    }
    if (admitted) {
      if (any_blocked_) {
        assert(cycle >= first_any_);
        resolve(any_to_admission, cycle - first_any_);
      }
      active_ = false;
    }
  }
  void sample(unsigned wb, unsigned wad, unsigned descriptors,
              unsigned physical_resident, unsigned bound_tags,
              unsigned pending_landings) {
    if (!enabled) return;
    ++sampled_cycles; wb_occupancy_sum += wb; wad_occupancy_sum += wad;
    wb_peak = std::max(wb_peak, wb); wad_peak = std::max(wad_peak, wad);
    descriptor_peak = std::max(descriptor_peak, descriptors);
    physical_resident_peak = std::max(physical_resident_peak, physical_resident);
    bound_tag_peak = std::max(bound_tag_peak, bound_tags);
    pending_landing_peak = std::max(pending_landing_peak, pending_landings);
  }
  void lower_accept(bool write, bool wb, unsigned bytes) {
    if (!enabled) return;
    if (write) { ++lower_writes; lower_write_bytes += bytes; }
    else { ++lower_reads; lower_read_bytes += bytes; }
    if (wb) lower_wb_bytes += bytes;
  }
  void print(FILE *fp, unsigned slice) const {
    fprintf(fp, "EPL2MSV1|OBSERVER|slice=%u|enabled=%u|sampled_cycles=%llu|wb_sum=%llu|wb_peak=%u|wad_sum=%llu|wad_peak=%u|descriptor_peak=%u|physical_resident_peak=%u|bound_tag_peak=%u|pending_landing_peak=%u|lower_reads=%llu|lower_read_bytes=%llu|lower_writes=%llu|lower_write_bytes=%llu|lower_wb_bytes=%llu|unfinished_head=%u|head_changes_without_admission=%llu|unclassified_refusal_observations=%llu\n",
        slice, enabled, sampled_cycles, wb_occupancy_sum, wb_peak,
        wad_occupancy_sum, wad_peak, descriptor_peak, physical_resident_peak,
        bound_tag_peak, pending_landing_peak, lower_reads, lower_read_bytes,
        lower_writes, lower_write_bytes, lower_wb_bytes, active_,
        head_changes_without_admission, unclassified_refusal_observations);
    if (!enabled) return;
    for (unsigned r = 0; r <= WAIT_REASONS; ++r) {
      const Wait &w = r == WAIT_REASONS ? any_to_admission : waits[r];
      fprintf(fp, "EPL2MSV1|WAIT|slice=%u|reason=%u|requests=%llu|observations=%llu|resolved=%llu|sum=%llu|max=%llu|unknown_while_unresolved=%llu|hist_log2=",
          slice, r, w.requests, w.observations, w.resolved, w.sum, w.maximum, w.unknown_while_unresolved);
      for (unsigned b = 0; b < k_bins; ++b)
        fprintf(fp, "%s%llu", b ? "," : "", w.histogram[b]);
      fprintf(fp, "\n");
    }
  }

 private:
  bool active_ = false, any_blocked_ = false;
  unsigned uid_ = 0;
  count seen_ = 0, unresolved_ = 0, first_any_ = 0;
  std::array<count, WAIT_REASONS> first_{};
};
}  // namespace mechanism_v1
}  // namespace ep_l2
#endif
