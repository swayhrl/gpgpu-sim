// LateBind-L2 Phase 1 observation-only statistics.
//
// This collector deliberately owns no cache resource and has no admission
// decision API.  Keeping it observational is what makes stats-off equivalence
// meaningful for the frozen baseline.

#ifndef GPGPUSIM_L2_LATEBIND_STATS_H
#define GPGPUSIM_L2_LATEBIND_STATS_H

#include <map>
#include <string>
#include <vector>

class mem_fetch;

class l2_latebind_stats {
 public:
  l2_latebind_stats(const char *cache_name, unsigned subpartition_id);
  ~l2_latebind_stats();

  void record_probe(unsigned status);
  // Record every L2 offer, including offers rejected for a transient
  // reservation shortage.  This retains the first-offer timestamp until the
  // request is eventually accepted.
  void record_offer(mem_fetch *mf, unsigned long long time,
                    unsigned probe_status, unsigned access_status);
  void record_reply(mem_fetch *mf, unsigned long long time);
  void record_consumed(mem_fetch *mf, unsigned long long time);
  void record_mshr(bool hit, bool available);
  void record_reservation(unsigned cache_index, unsigned sector_mask,
                          unsigned long long time);
  void record_fill(unsigned cache_index, unsigned sector_mask,
                   unsigned long long time);
  void record_reservation_ready(unsigned cache_index, unsigned sector_mask,
                                unsigned long long time);
  void sample(unsigned resident_sectors, unsigned reserved_sectors,
              unsigned dirty_sectors, bool data_port_busy,
              bool fill_port_busy);
  void record_writeback_enqueue(mem_fetch *mf, unsigned long long time,
                                unsigned bytes, unsigned sectors);
  void record_lower_request(mem_fetch *mf, unsigned long long time);
  void record_frc_lower_request(mem_fetch *fetch, mem_fetch *primary,
                                unsigned long long time);
  void record_lower_return(mem_fetch *mf, unsigned long long time);

  void print(FILE *fp) const;
  static void print_global(FILE *fp);

 private:
  struct request_record {
    unsigned long long offer_time;
    unsigned long long accept_time;
    unsigned probe_status;
    unsigned access_status;
    bool accepted;
    bool lower_issued;
    bool lower_returned;
    unsigned long long lower_issue_time;
    unsigned long long lower_return_time;
  };
  struct writeback_record {
    unsigned long long enqueue_time;
    unsigned bytes;
    unsigned sectors;
  };

  typedef std::map<mem_fetch *, request_record> request_map;
  typedef std::map<mem_fetch *, writeback_record> writeback_map;
  typedef std::map<mem_fetch *, mem_fetch *> frc_fetch_map;
  typedef std::map<unsigned long long, unsigned long long> histogram;

  static unsigned long long lifetime_bucket(unsigned long long cycles);
  static void add_histogram(histogram &dst, const histogram &src);
  static void print_histogram(FILE *fp, const char *name,
                              const histogram &hist);
  void record_latency(mem_fetch *mf, unsigned long long time);

  std::string m_cache_name;
  unsigned m_subpartition_id;
  unsigned long long m_probe[5];
  unsigned long long m_accept[3];
  unsigned long long m_mshr_new;
  unsigned long long m_mshr_merge;
  unsigned long long m_mshr_entry_full;
  unsigned long long m_mshr_merge_full;
  unsigned long long m_reservation_alloc;
  unsigned long long m_reservation_fill;
  unsigned long long m_reservation_direct_ready;
  unsigned long long m_fill_without_reservation;
  unsigned long long m_sample_cycles;
  unsigned long long m_resident_sector_cycles;
  unsigned long long m_reserved_sector_cycles;
  unsigned long long m_dirty_sector_cycles;
  unsigned long long m_resident_sector_max;
  unsigned long long m_reserved_sector_max;
  unsigned long long m_dirty_sector_max;
  unsigned long long m_data_port_busy_cycles;
  unsigned long long m_fill_port_busy_cycles;
  unsigned long long m_writeback_enqueued;
  unsigned long long m_writeback_lower_accepted;
  unsigned long long m_writeback_bytes;
  unsigned long long m_writeback_sectors;
  unsigned long long m_writeback_queue_cycles;
  unsigned long long m_writeback_queue_max;
  unsigned long long m_lower_read_delay_count;
  unsigned long long m_lower_read_pre_offer_cycles;
  unsigned long long m_lower_read_pre_mem_cycles;
  unsigned long long m_lower_read_mem_cycles;
  unsigned long long m_lower_read_post_mem_cycles;
  unsigned long long m_unresolved_requests;
  unsigned long long m_unresolved_reservations;
  std::map<unsigned long long, unsigned long long> m_reservation_start;
  request_map m_requests;
  writeback_map m_writebacks;
  frc_fetch_map m_frc_fetch_primary;
  histogram m_reservation_lifetime;
  histogram m_request_latency[3];

  static std::vector<l2_latebind_stats *> s_instances;
};

#endif
