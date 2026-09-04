// Bounded, behavior-neutral M4C/M4B memory-hierarchy telemetry.
#ifndef GPGPU_SIM_MEMORY_TELEMETRY_H
#define GPGPU_SIM_MEMORY_TELEMETRY_H

#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "../abstract_hardware_model.h"

class m4c_memory_telemetry {
 public:
  m4c_memory_telemetry();
  void configure(unsigned level, uint64_t window_transactions);
  void reset();
  bool enabled() const { return m_level != 0; }
  unsigned level() const { return m_level; }
  void record_l1(unsigned memory_class, unsigned cache_status,
                 unsigned bytes, unsigned sid,
                 unsigned translation_outcome = M4C_TRANSLATION_UNOBSERVED);
  void record_l2(unsigned memory_class, unsigned cache_status,
                 unsigned bytes, unsigned subpartition,
                 unsigned translation_outcome = M4C_TRANSLATION_UNOBSERVED,
                 unsigned l1_cache_status =
                     M4C_TELEMETRY_L1_STATUS_UNAVAILABLE);
  void record_frontend_instruction(unsigned active_lanes);
  void record_frontend_transaction(unsigned memory_class,
                                   unsigned request_operation,
                                   unsigned requested_bytes,
                                   unsigned transaction_bytes,
                                   unsigned sector_population);
  void record_l2_replacement(unsigned incoming_class, unsigned victim_class,
                             unsigned subpartition);
  void record_dram(unsigned memory_class, unsigned bytes, bool is_write,
                   unsigned channel);
  void record_l2_queue(unsigned icnt_to_l2, unsigned l2_to_dram,
                       unsigned dram_to_l2, unsigned l2_to_icnt,
                       unsigned subpartition);
  void print(FILE *fout, const char *kernel_name) const;
  // Called only after the existing simulator has finished and printed one
  // kernel.  It rolls observational state into the next kernel without
  // touching any simulated cache/TLB/memory state.
  void finish_kernel();

 private:
  enum { kCacheStatuses = 6 };
  enum { kTranslationOutcomes = M4C_TRANSLATION_OUTCOME_COUNT };
  enum { kRequestOperations = 3 };
  struct counters {
    uint64_t frontend_memory_instructions;
    uint64_t frontend_active_lane_references;
    uint64_t frontend_transactions[M4C_MEMORY_CLASS_COUNT]
                                  [kRequestOperations];
    uint64_t frontend_requested_bytes[M4C_MEMORY_CLASS_COUNT]
                                      [kRequestOperations];
    uint64_t frontend_transaction_bytes[M4C_MEMORY_CLASS_COUNT]
                                        [kRequestOperations];
    uint64_t frontend_sector_population[M4C_MEMORY_CLASS_COUNT]
                                        [kRequestOperations];
    uint64_t l1_status[M4C_MEMORY_CLASS_COUNT][kCacheStatuses];
    uint64_t l1_bytes[M4C_MEMORY_CLASS_COUNT];
    uint64_t l2_status[M4C_MEMORY_CLASS_COUNT][kCacheStatuses];
    uint64_t l2_bytes[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_requests[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_bytes[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_read_requests[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_write_requests[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_read_bytes[M4C_MEMORY_CLASS_COUNT];
    uint64_t dram_write_bytes[M4C_MEMORY_CLASS_COUNT];
    uint64_t l2_replacements[M4C_MEMORY_CLASS_COUNT]
                            [M4C_MEMORY_CLASS_COUNT];
    uint64_t cross_l1[M4C_MEMORY_CLASS_COUNT][kTranslationOutcomes]
                     [kCacheStatuses];
    uint64_t cross_l1_l2[M4C_MEMORY_CLASS_COUNT][kTranslationOutcomes]
                        [kCacheStatuses][kCacheStatuses];
    uint64_t l2_queue_samples;
    uint64_t l2_icnt_to_l2_total;
    uint64_t l2_to_dram_total;
    uint64_t dram_to_l2_total;
    uint64_t l2_to_icnt_total;
    uint64_t l2_icnt_to_l2_high_water;
    uint64_t l2_to_dram_high_water;
    uint64_t dram_to_l2_high_water;
    uint64_t l2_to_icnt_high_water;
    counters();
  };
  void record_window_transaction();
  void add_frontend_instruction(counters *destination, unsigned active_lanes);
  void add_frontend_transaction(counters *destination, unsigned memory_class,
                                unsigned request_operation,
                                unsigned requested_bytes,
                                unsigned transaction_bytes,
                                unsigned sector_population);
  void add_l1(counters *destination, unsigned memory_class,
              unsigned cache_status, unsigned bytes,
              unsigned translation_outcome);
  void add_l2(counters *destination, unsigned memory_class,
              unsigned cache_status, unsigned bytes,
              unsigned translation_outcome, unsigned l1_cache_status);
  void add_dram(counters *destination, unsigned memory_class, unsigned bytes,
                bool is_write);
  void add_l2_replacement(counters *destination, unsigned incoming_class,
                          unsigned victim_class);
  void print_counters(FILE *fout, const counters &stats, const char *scope,
                      unsigned index, const char *kernel_name) const;
  unsigned m_level;
  unsigned m_kernel_index;
  uint64_t m_window_transactions;
  uint64_t m_transactions_in_window;
  counters m_total;
  counters m_window;
  std::vector<counters> m_completed_windows;
};

m4c_memory_telemetry &m4c_memory_telemetry_instance();
const char *m4c_memory_telemetry_class_name(unsigned memory_class);
const char *m4c_memory_telemetry_cache_status_name(unsigned status);
const char *m4c_memory_telemetry_translation_outcome_name(unsigned outcome);
const char *m4c_memory_telemetry_request_operation_name(unsigned operation);

#endif
