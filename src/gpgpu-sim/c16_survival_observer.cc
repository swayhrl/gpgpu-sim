#include "c16_survival_observer.h"

#include <fstream>
#include <stdexcept>

namespace c16_survival_observer {

generation_observer::generation_observer()
    : enabled_(false), target_class_(1), snapshot_after_uid_(0),
      d2_begin_uid_(0), d2_mid_uid_(0), d2_end_uid_(0), serial_(0),
      d2_active_(false), old_generation_d2_hits_(0),
      old_generation_d2_reloads_(0) {}

void generation_observer::configure(bool enabled, const char *path,
                                    unsigned target_class,
                                    unsigned snapshot_after_uid,
                                    unsigned d2_begin_uid,
                                    unsigned d2_mid_uid,
                                    unsigned d2_end_uid) {
  enabled_ = enabled;
  path_ = path ? path : "";
  target_class_ = target_class;
  snapshot_after_uid_ = snapshot_after_uid;
  d2_begin_uid_ = d2_begin_uid;
  d2_mid_uid_ = d2_mid_uid;
  d2_end_uid_ = d2_end_uid;
  serial_ = 0;
  d2_active_ = false;
  old_generation_d2_hits_ = 0;
  old_generation_d2_reloads_ = 0;
  states_.clear();
  snapshot_.clear();
  evicted_then_refilled_.clear();
  if (!enabled_) return;
  if (path_.empty() || target_class_ == 0)
    throw std::runtime_error("enabled survival observer requires log and class");
  write_header();
}

void generation_observer::write_header() {
  std::ofstream out(path_.c_str(), std::ios::out | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot create survival observer log");
  out << "label\tlaunch_uid\tsnapshot_population\t"
         "exact_old_generation_survivors\t"
         "exact_old_generation_survival_fraction\t"
         "current_new_generation_residents\tcurrent_absent\t"
         "evicted_then_refilled_same_address\told_generation_D2_hits\t"
         "old_generation_D2_reloads\taccounting_closed\n";
}

void generation_observer::on_line_allocate(uint64_t address,
                                           bool protected_line,
                                           unsigned target_class) {
  if (!enabled_) return;
  state &entry = states_[line(address)];
  entry.pending = true;
  entry.pending_protected = protected_line;
  entry.pending_target_class = target_class;
}

void generation_observer::on_fill(uint64_t address) {
  if (!enabled_) return;
  const uint64_t key = line(address);
  state &entry = states_[key];
  if (!entry.pending) return;  // sector-child fill, not a new line generation
  entry.generation = ++serial_;
  entry.resident = true;
  entry.protected_line = entry.pending_protected;
  entry.target_class = entry.pending_target_class;
  entry.pending = false;
  entry.pending_protected = false;
  entry.pending_target_class = 0;
  std::unordered_map<uint64_t, uint64_t>::const_iterator old =
      snapshot_.find(key);
  if (old != snapshot_.end() && entry.generation != old->second) {
    evicted_then_refilled_.insert(key);
    if (d2_active_) ++old_generation_d2_reloads_;
  }
}

void generation_observer::on_evict(uint64_t address) {
  if (!enabled_) return;
  state &entry = states_[line(address)];
  entry.resident = false;
  entry.protected_line = false;
  entry.target_class = 0;
  entry.pending = false;
  entry.pending_protected = false;
  entry.pending_target_class = 0;
}

void generation_observer::on_true_hit(uint64_t address) {
  if (!enabled_ || !d2_active_) return;
  const uint64_t key = line(address);
  std::unordered_map<uint64_t, uint64_t>::const_iterator old =
      snapshot_.find(key);
  if (old == snapshot_.end()) return;
  std::unordered_map<uint64_t, state>::const_iterator current =
      states_.find(key);
  if (current != states_.end() && current->second.resident &&
      current->second.generation == old->second)
    ++old_generation_d2_hits_;
}

void generation_observer::freeze_snapshot() {
  if (!enabled_) return;
  snapshot_.clear();
  for (std::unordered_map<uint64_t, state>::const_iterator it = states_.begin();
       it != states_.end(); ++it) {
    const state &entry = it->second;
    if (entry.resident && entry.protected_line &&
        entry.target_class == target_class_)
      snapshot_[it->first] = entry.generation;
  }
  evicted_then_refilled_.clear();
  old_generation_d2_hits_ = 0;
  old_generation_d2_reloads_ = 0;
}

report generation_observer::current_report() const {
  report result = {};
  result.snapshot_population = snapshot_.size();
  for (std::unordered_map<uint64_t, uint64_t>::const_iterator it =
           snapshot_.begin();
       it != snapshot_.end(); ++it) {
    std::unordered_map<uint64_t, state>::const_iterator current =
        states_.find(it->first);
    if (current == states_.end() || !current->second.resident) {
      ++result.current_absent;
    } else if (current->second.generation == it->second) {
      ++result.exact_old_generation_survivors;
    } else {
      ++result.current_new_generation_residents;
    }
  }
  result.evicted_then_refilled_same_address = evicted_then_refilled_.size();
  result.old_generation_d2_hits = old_generation_d2_hits_;
  result.old_generation_d2_reloads = old_generation_d2_reloads_;
  result.accounting_closed =
      result.snapshot_population == result.exact_old_generation_survivors +
                                        result.current_new_generation_residents +
                                        result.current_absent;
  return result;
}

void generation_observer::emit(const char *label, unsigned launch_uid) const {
  if (!enabled_) return;
  const report value = current_report();
  std::ofstream out(path_.c_str(), std::ios::out | std::ios::app);
  if (!out) throw std::runtime_error("cannot append survival observer log");
  const double fraction = value.snapshot_population
                              ? static_cast<double>(
                                    value.exact_old_generation_survivors) /
                                    value.snapshot_population
                              : 0.0;
  out << label << '\t' << launch_uid << '\t' << value.snapshot_population
      << '\t' << value.exact_old_generation_survivors << '\t' << fraction
      << '\t' << value.current_new_generation_residents << '\t'
      << value.current_absent << '\t'
      << value.evicted_then_refilled_same_address << '\t'
      << value.old_generation_d2_hits << '\t'
      << value.old_generation_d2_reloads << '\t'
      << (value.accounting_closed ? 1 : 0) << '\n';
}

void generation_observer::kernel_begin(unsigned launch_uid) {
  if (!enabled_) return;
  if (launch_uid == d2_begin_uid_) {
    d2_active_ = true;
    emit("D2_L0_BEFORE", launch_uid);
  }
  if (launch_uid == d2_mid_uid_) emit("D2_L0_MID", launch_uid);
}

void generation_observer::kernel_end(unsigned launch_uid) {
  if (!enabled_) return;
  if (launch_uid == snapshot_after_uid_) {
    freeze_snapshot();
    emit("D1_L0_SNAPSHOT", launch_uid);
  }
  if (launch_uid == d2_end_uid_) {
    emit("D2_L0_END", launch_uid);
    d2_active_ = false;
  }
}

uint64_t generation_observer::generation(uint64_t address) const {
  std::unordered_map<uint64_t, state>::const_iterator it =
      states_.find(line(address));
  return it == states_.end() ? 0 : it->second.generation;
}

generation_observer &global() {
  static generation_observer instance;
  return instance;
}
void configure(bool enabled, const char *path, unsigned target_class,
               unsigned snapshot_after_uid, unsigned d2_begin_uid,
               unsigned d2_mid_uid, unsigned d2_end_uid) {
  global().configure(enabled, path, target_class, snapshot_after_uid,
                     d2_begin_uid, d2_mid_uid, d2_end_uid);
}
void on_line_allocate(uint64_t address, bool protected_line,
                      unsigned target_class) {
  global().on_line_allocate(address, protected_line, target_class);
}
void on_fill(uint64_t address) { global().on_fill(address); }
void on_evict(uint64_t address) { global().on_evict(address); }
void on_true_hit(uint64_t address) { global().on_true_hit(address); }
void kernel_begin(unsigned launch_uid) { global().kernel_begin(launch_uid); }
void kernel_end(unsigned launch_uid) { global().kernel_end(launch_uid); }

}  // namespace c16_survival_observer
