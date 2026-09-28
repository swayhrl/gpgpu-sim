#ifndef C16_SURVIVAL_OBSERVER_H
#define C16_SURVIVAL_OBSERVER_H

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace c16_survival_observer {

struct report {
  uint64_t snapshot_population;
  uint64_t exact_old_generation_survivors;
  uint64_t current_new_generation_residents;
  uint64_t current_absent;
  uint64_t evicted_then_refilled_same_address;
  uint64_t old_generation_d2_hits;
  uint64_t old_generation_d2_reloads;
  bool accounting_closed;
};

class generation_observer {
 public:
  generation_observer();
  void configure(bool enabled, const char *path, unsigned target_class,
                 unsigned snapshot_after_uid, unsigned d2_begin_uid,
                 unsigned d2_mid_uid, unsigned d2_end_uid);
  void on_line_allocate(uint64_t address, bool protected_line,
                        unsigned target_class);
  void on_fill(uint64_t address);
  void on_evict(uint64_t address);
  void on_true_hit(uint64_t address);
  void kernel_begin(unsigned launch_uid);
  void kernel_end(unsigned launch_uid);
  void freeze_snapshot();
  report current_report() const;
  void emit(const char *label, unsigned launch_uid) const;
  uint64_t generation(uint64_t address) const;
  bool enabled() const { return enabled_; }

 private:
  struct state {
    state()
        : generation(0), resident(false), protected_line(false),
          target_class(0), pending(false), pending_protected(false),
          pending_target_class(0) {}
    uint64_t generation;
    bool resident;
    bool protected_line;
    unsigned target_class;
    bool pending;
    bool pending_protected;
    unsigned pending_target_class;
  };

  uint64_t line(uint64_t address) const { return address & ~uint64_t(127); }
  void write_header();

  bool enabled_;
  std::string path_;
  unsigned target_class_;
  unsigned snapshot_after_uid_;
  unsigned d2_begin_uid_;
  unsigned d2_mid_uid_;
  unsigned d2_end_uid_;
  uint64_t serial_;
  bool d2_active_;
  uint64_t old_generation_d2_hits_;
  uint64_t old_generation_d2_reloads_;
  std::unordered_map<uint64_t, state> states_;
  std::unordered_map<uint64_t, uint64_t> snapshot_;
  std::unordered_set<uint64_t> evicted_then_refilled_;
};

generation_observer &global();
void configure(bool enabled, const char *path, unsigned target_class,
               unsigned snapshot_after_uid, unsigned d2_begin_uid,
               unsigned d2_mid_uid, unsigned d2_end_uid);
void on_line_allocate(uint64_t address, bool protected_line,
                      unsigned target_class);
void on_fill(uint64_t address);
void on_evict(uint64_t address);
void on_true_hit(uint64_t address);
void kernel_begin(unsigned launch_uid);
void kernel_end(unsigned launch_uid);

}  // namespace c16_survival_observer

#endif
