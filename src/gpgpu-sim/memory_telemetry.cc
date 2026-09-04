#include "memory_telemetry.h"

#include <assert.h>
#include <string.h>

m4c_memory_telemetry::counters::counters()
    : frontend_memory_instructions(0), frontend_active_lane_references(0),
      frontend_transactions(), frontend_requested_bytes(),
      frontend_transaction_bytes(), frontend_sector_population(), l1_status(),
      l1_bytes(), l2_status(), l2_bytes(), dram_requests(), dram_bytes(),
      dram_read_requests(), dram_write_requests(), dram_read_bytes(),
      dram_write_bytes(), l2_replacements(), cross_l1(), cross_l1_l2(),
      l2_queue_samples(0), l2_icnt_to_l2_total(0),
      l2_to_dram_total(0), dram_to_l2_total(0), l2_to_icnt_total(0),
      l2_icnt_to_l2_high_water(0), l2_to_dram_high_water(0),
      dram_to_l2_high_water(0), l2_to_icnt_high_water(0) {}

m4c_memory_telemetry::m4c_memory_telemetry()
    : m_level(0), m_kernel_index(0), m_window_transactions(0),
      m_transactions_in_window(0), m_total(), m_window(),
      m_completed_windows() {}

void m4c_memory_telemetry::configure(unsigned level,
                                      uint64_t window_transactions) {
  assert(level <= 3);
  assert(level < 2 || window_transactions != 0);
  m_level = level;
  m_window_transactions = window_transactions;
  reset();
}

void m4c_memory_telemetry::reset() {
  m_kernel_index = 0;
  m_transactions_in_window = 0;
  m_total = counters();
  m_window = counters();
  m_completed_windows.clear();
}

void m4c_memory_telemetry::add_frontend_instruction(
    counters *destination, unsigned active_lanes) {
  assert(destination != 0);
  ++destination->frontend_memory_instructions;
  destination->frontend_active_lane_references += active_lanes;
}

void m4c_memory_telemetry::add_frontend_transaction(
    counters *destination, unsigned memory_class, unsigned request_operation,
    unsigned requested_bytes, unsigned transaction_bytes,
    unsigned sector_population) {
  assert(destination != 0 && memory_class < M4C_MEMORY_CLASS_COUNT &&
         request_operation < kRequestOperations);
  ++destination->frontend_transactions[memory_class][request_operation];
  destination->frontend_requested_bytes[memory_class][request_operation] +=
      requested_bytes;
  destination->frontend_transaction_bytes[memory_class][request_operation] +=
      transaction_bytes;
  destination->frontend_sector_population[memory_class][request_operation] +=
      sector_population;
}

void m4c_memory_telemetry::add_l1(counters *destination,
                                   unsigned memory_class,
                                   unsigned cache_status, unsigned bytes,
                                   unsigned translation_outcome) {
  assert(destination != 0 && memory_class < M4C_MEMORY_CLASS_COUNT &&
         cache_status < kCacheStatuses &&
         translation_outcome < kTranslationOutcomes);
  ++destination->l1_status[memory_class][cache_status];
  destination->l1_bytes[memory_class] += bytes;
  if (memory_class <= M4C_DATA_KV_CACHE)
    ++destination->cross_l1[memory_class][translation_outcome][cache_status];
}

void m4c_memory_telemetry::add_l2(counters *destination,
                                   unsigned memory_class,
                                   unsigned cache_status, unsigned bytes,
                                   unsigned translation_outcome,
                                   unsigned l1_cache_status) {
  assert(destination != 0 && memory_class < M4C_MEMORY_CLASS_COUNT &&
         cache_status < kCacheStatuses &&
         translation_outcome < kTranslationOutcomes);
  ++destination->l2_status[memory_class][cache_status];
  destination->l2_bytes[memory_class] += bytes;
  if (memory_class <= M4C_DATA_KV_CACHE &&
      l1_cache_status < kCacheStatuses)
    ++destination
          ->cross_l1_l2[memory_class][translation_outcome]
                       [l1_cache_status][cache_status];
}

void m4c_memory_telemetry::add_dram(counters *destination,
                                     unsigned memory_class, unsigned bytes,
                                     bool is_write) {
  assert(destination != 0 && memory_class < M4C_MEMORY_CLASS_COUNT);
  ++destination->dram_requests[memory_class];
  destination->dram_bytes[memory_class] += bytes;
  if (is_write) {
    ++destination->dram_write_requests[memory_class];
    destination->dram_write_bytes[memory_class] += bytes;
  } else {
    ++destination->dram_read_requests[memory_class];
    destination->dram_read_bytes[memory_class] += bytes;
  }
}

void m4c_memory_telemetry::add_l2_replacement(
    counters *destination, unsigned incoming_class, unsigned victim_class) {
  assert(destination != 0 && incoming_class < M4C_MEMORY_CLASS_COUNT &&
         victim_class < M4C_MEMORY_CLASS_COUNT);
  ++destination->l2_replacements[incoming_class][victim_class];
}

void m4c_memory_telemetry::record_window_transaction() {
  if (m_level < 2) return;
  ++m_transactions_in_window;
  if (m_transactions_in_window == m_window_transactions) {
    m_completed_windows.push_back(m_window);
    m_window = counters();
    m_transactions_in_window = 0;
  }
}

void m4c_memory_telemetry::record_l1(unsigned memory_class,
                                      unsigned cache_status, unsigned bytes,
                                      unsigned sid,
                                      unsigned translation_outcome) {
  (void)sid;
  if (!enabled()) return;
  add_l1(&m_total, memory_class, cache_status, bytes, translation_outcome);
  if (m_level >= 2)
    add_l1(&m_window, memory_class, cache_status, bytes,
           translation_outcome);
  record_window_transaction();
}

void m4c_memory_telemetry::record_l2(unsigned memory_class,
                                      unsigned cache_status, unsigned bytes,
                                      unsigned subpartition,
                                      unsigned translation_outcome,
                                      unsigned l1_cache_status) {
  (void)subpartition;
  if (!enabled()) return;
  add_l2(&m_total, memory_class, cache_status, bytes, translation_outcome,
         l1_cache_status);
  if (m_level >= 2)
    add_l2(&m_window, memory_class, cache_status, bytes, translation_outcome,
           l1_cache_status);
}

void m4c_memory_telemetry::record_frontend_instruction(unsigned active_lanes) {
  if (!enabled()) return;
  add_frontend_instruction(&m_total, active_lanes);
}

void m4c_memory_telemetry::record_frontend_transaction(
    unsigned memory_class, unsigned request_operation, unsigned requested_bytes,
    unsigned transaction_bytes, unsigned sector_population) {
  if (!enabled()) return;
  add_frontend_transaction(&m_total, memory_class, request_operation,
                           requested_bytes, transaction_bytes,
                           sector_population);
  if (m_level >= 2)
    add_frontend_transaction(&m_window, memory_class, request_operation,
                             requested_bytes, transaction_bytes,
                             sector_population);
}

void m4c_memory_telemetry::record_l2_replacement(unsigned incoming_class,
                                                   unsigned victim_class,
                                                   unsigned subpartition) {
  (void)subpartition;
  if (!enabled()) return;
  add_l2_replacement(&m_total, incoming_class, victim_class);
  if (m_level >= 2)
    add_l2_replacement(&m_window, incoming_class, victim_class);
}

void m4c_memory_telemetry::record_dram(unsigned memory_class, unsigned bytes,
                                        bool is_write, unsigned channel) {
  (void)channel;
  if (!enabled()) return;
  add_dram(&m_total, memory_class, bytes, is_write);
  if (m_level >= 2) add_dram(&m_window, memory_class, bytes, is_write);
}

void m4c_memory_telemetry::record_l2_queue(
    unsigned icnt_to_l2, unsigned l2_to_dram, unsigned dram_to_l2,
    unsigned l2_to_icnt, unsigned subpartition) {
  (void)subpartition;
  if (m_level < 2) return;
  counters *destinations[] = {&m_total, &m_window};
  for (unsigned destination = 0; destination != 2; ++destination) {
    counters *stats = destinations[destination];
    ++stats->l2_queue_samples;
    stats->l2_icnt_to_l2_total += icnt_to_l2;
    stats->l2_to_dram_total += l2_to_dram;
    stats->dram_to_l2_total += dram_to_l2;
    stats->l2_to_icnt_total += l2_to_icnt;
    if (icnt_to_l2 > stats->l2_icnt_to_l2_high_water)
      stats->l2_icnt_to_l2_high_water = icnt_to_l2;
    if (l2_to_dram > stats->l2_to_dram_high_water)
      stats->l2_to_dram_high_water = l2_to_dram;
    if (dram_to_l2 > stats->dram_to_l2_high_water)
      stats->dram_to_l2_high_water = dram_to_l2;
    if (l2_to_icnt > stats->l2_to_icnt_high_water)
      stats->l2_to_icnt_high_water = l2_to_icnt;
  }
}

void m4c_memory_telemetry::print_counters(
    FILE *fout, const counters &stats, const char *scope, unsigned index,
    const char *kernel_name) const {
  fprintf(fout, "m4c_telemetry_frontend_instruction\t%s\t%s\t%u\t%llu\t%llu\n",
          scope, kernel_name, index,
          (unsigned long long)stats.frontend_memory_instructions,
          (unsigned long long)stats.frontend_active_lane_references);
  for (unsigned memory_class = 0; memory_class < M4C_MEMORY_CLASS_COUNT;
       ++memory_class) {
    for (unsigned operation = 0; operation < kRequestOperations;
         ++operation)
      fprintf(fout,
              "m4c_telemetry_frontend_transaction\t%s\t%s\t%u\t%s\t%s\t%llu\t%llu\t%llu\t%llu\n",
              scope, kernel_name, index,
              m4c_memory_telemetry_class_name(memory_class),
              m4c_memory_telemetry_request_operation_name(operation),
              (unsigned long long)
                  stats.frontend_transactions[memory_class][operation],
              (unsigned long long)
                  stats.frontend_requested_bytes[memory_class][operation],
              (unsigned long long)
                  stats.frontend_transaction_bytes[memory_class][operation],
              (unsigned long long)
                  stats.frontend_sector_population[memory_class][operation]);
    for (unsigned status = 0; status < 6; ++status) {
      fprintf(fout, "m4c_telemetry\t%s\t%s\t%u\t%s\t%s\t%llu\n", scope,
              kernel_name, index,
              m4c_memory_telemetry_class_name(memory_class),
              m4c_memory_telemetry_cache_status_name(status),
              (unsigned long long)stats.l1_status[memory_class][status]);
      fprintf(fout, "m4c_telemetry_l2\t%s\t%s\t%u\t%s\t%s\t%llu\n", scope,
              kernel_name, index,
              m4c_memory_telemetry_class_name(memory_class),
              m4c_memory_telemetry_cache_status_name(status),
              (unsigned long long)stats.l2_status[memory_class][status]);
    }
    fprintf(fout, "m4c_telemetry_dram\t%s\t%s\t%u\t%s\t%llu\t%llu\n",
            scope, kernel_name, index,
            m4c_memory_telemetry_class_name(memory_class),
            (unsigned long long)stats.dram_requests[memory_class],
            (unsigned long long)stats.dram_bytes[memory_class]);
    fprintf(fout,
            "m4c_telemetry_bytes\t%s\t%s\t%u\t%s\t%llu\t%llu\n",
            scope, kernel_name, index,
            m4c_memory_telemetry_class_name(memory_class),
            (unsigned long long)stats.l1_bytes[memory_class],
            (unsigned long long)stats.l2_bytes[memory_class]);
    fprintf(fout,
            "m4c_telemetry_dram_rw\t%s\t%s\t%u\t%s\t%llu\t%llu\t%llu\t%llu\n",
            scope, kernel_name, index,
            m4c_memory_telemetry_class_name(memory_class),
            (unsigned long long)stats.dram_read_requests[memory_class],
            (unsigned long long)stats.dram_read_bytes[memory_class],
            (unsigned long long)stats.dram_write_requests[memory_class],
            (unsigned long long)stats.dram_write_bytes[memory_class]);
    for (unsigned victim_class = 0; victim_class < M4C_MEMORY_CLASS_COUNT;
         ++victim_class)
      fprintf(fout,
              "m4c_telemetry_l2_replacement\t%s\t%s\t%u\t%s\t%s\t%llu\n",
              scope, kernel_name, index,
              m4c_memory_telemetry_class_name(memory_class),
              m4c_memory_telemetry_class_name(victim_class),
              (unsigned long long)
                  stats.l2_replacements[memory_class][victim_class]);
  }
  fprintf(fout, "m4c_telemetry_l2_queue\t%s\t%s\t%u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
          scope, kernel_name, index,
          (unsigned long long)stats.l2_queue_samples,
          (unsigned long long)stats.l2_icnt_to_l2_total,
          (unsigned long long)stats.l2_to_dram_total,
          (unsigned long long)stats.dram_to_l2_total,
          (unsigned long long)stats.l2_to_icnt_total,
          (unsigned long long)stats.l2_icnt_to_l2_high_water,
          (unsigned long long)stats.l2_to_dram_high_water,
          (unsigned long long)stats.dram_to_l2_high_water,
          (unsigned long long)stats.l2_to_icnt_high_water);
  // Sparse aggregate matrices retain exact correlations without a
  // per-transaction stream or a dense all-zero formal log.
  for (unsigned object = M4C_DATA_UNKNOWN; object <= M4C_DATA_KV_CACHE;
       ++object)
    for (unsigned outcome = 0; outcome < kTranslationOutcomes; ++outcome)
      for (unsigned l1_status = 0; l1_status < kCacheStatuses; ++l1_status) {
        const uint64_t l1 = stats.cross_l1[object][outcome][l1_status];
        if (l1 != 0)
          fprintf(fout,
                  "m4c_telemetry_cross_l1\t%s\t%s\t%u\t%s\t%s\t%s\t%llu\n",
                  scope, kernel_name, index,
                  m4c_memory_telemetry_class_name(object),
                  m4c_memory_telemetry_translation_outcome_name(outcome),
                  m4c_memory_telemetry_cache_status_name(l1_status),
                  (unsigned long long)l1);
        for (unsigned l2_status = 0; l2_status < kCacheStatuses;
             ++l2_status) {
          const uint64_t value =
              stats.cross_l1_l2[object][outcome][l1_status][l2_status];
          if (value != 0)
            fprintf(fout,
                    "m4c_telemetry_cross_l1_l2\t%s\t%s\t%u\t%s\t%s\t%s\t%s\t%llu\n",
                    scope, kernel_name, index,
                    m4c_memory_telemetry_class_name(object),
                    m4c_memory_telemetry_translation_outcome_name(outcome),
                    m4c_memory_telemetry_cache_status_name(l1_status),
                    m4c_memory_telemetry_cache_status_name(l2_status),
                    (unsigned long long)value);
        }
      }
}

void m4c_memory_telemetry::print(FILE *fout, const char *kernel_name) const {
  if (!enabled()) return;
  const char *name = kernel_name && kernel_name[0] ? kernel_name : "UNKNOWN";
  fprintf(fout, "m4c_telemetry_schema = M4C_MEMORY_TELEMETRY_V1\n");
  fprintf(fout, "m4c_telemetry_level = %u\n", m_level);
  fprintf(fout, "m4c_telemetry_kernel_index = %u\n", m_kernel_index);
  fprintf(fout, "m4c_telemetry_window_transactions = %llu\n",
          (unsigned long long)m_window_transactions);
  print_counters(fout, m_total, "KERNEL", m_kernel_index, name);
  for (unsigned index = 0; index < m_completed_windows.size(); ++index)
    print_counters(fout, m_completed_windows[index], "FIXED_WINDOW", index,
                   name);
  if (m_level >= 2 && m_transactions_in_window != 0)
    print_counters(fout, m_window, "FIXED_WINDOW_PARTIAL",
                   m_completed_windows.size(), name);
}

void m4c_memory_telemetry::finish_kernel() {
  if (!enabled()) return;
  ++m_kernel_index;
  m_transactions_in_window = 0;
  m_total = counters();
  m_window = counters();
  m_completed_windows.clear();
}

m4c_memory_telemetry &m4c_memory_telemetry_instance() {
  static m4c_memory_telemetry telemetry;
  return telemetry;
}

const char *m4c_memory_telemetry_class_name(unsigned memory_class) {
  static const char *names[] = {"DATA_UNKNOWN", "DATA_WEIGHT",
                                "DATA_KV_CACHE", "PTE_L0", "PTE_L1",
                                "PTE_L2", "PTE_L3", "OTHER"};
  assert(memory_class < M4C_MEMORY_CLASS_COUNT);
  return names[memory_class];
}

const char *m4c_memory_telemetry_cache_status_name(unsigned status) {
  static const char *names[] = {"HIT", "HIT_RESERVED", "MISS",
                                "RESERVATION_FAIL", "SECTOR_MISS",
                                "MSHR_HIT"};
  assert(status < 6);
  return names[status];
}

const char *m4c_memory_telemetry_translation_outcome_name(unsigned outcome) {
  static const char *names[] = {"UNOBSERVED", "VM_DISABLED",
                                "IDEAL_IDENTITY", "L1_TLB_HIT",
                                "L2_TLB_HIT", "PTW"};
  assert(outcome < M4C_TRANSLATION_OUTCOME_COUNT);
  return names[outcome];
}

const char *m4c_memory_telemetry_request_operation_name(unsigned operation) {
  static const char *names[] = {"LOAD", "STORE", "ATOMIC"};
  assert(operation < 3);
  return names[operation];
}
