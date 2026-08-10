#ifndef DECOUPLED_L2_CACHE_INCLUDED
#define DECOUPLED_L2_CACHE_INCLUDED

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "gpu-cache.h"

class memory_config;
class mem_fetch;
class mem_fetch_allocator;
class mem_fetch_interface;

// A metadata/timing L2 backend.  It intentionally shares GPGPU-Sim's normal
// mem_fetch and DRAM interface, while modelling V3-like token, AAD, OTF, WBQ,
// and bank ownership internally.  It has no data or ECC array.
class decoupled_l2_cache {
 public:
  decoupled_l2_cache(const char *name, l2_cache_config &cache_config,
                     const memory_config *memory_config,
                     mem_fetch_interface *memport,
                     mem_fetch_allocator *mf_allocator);

  enum cache_request_status access(new_addr_type addr, mem_fetch *mf,
                                   unsigned long long time,
                                   std::list<cache_event> &events);
  void cycle(unsigned long long time);
  bool waiting_for_fill(mem_fetch *mf) const;
  bool fill_port_free() const;
  void fill(mem_fetch *mf, unsigned long long time);
  bool access_ready() const { return !m_response_ready.empty(); }
  mem_fetch *next_access();
  bool data_port_free() const;

  void writeback_done(mem_fetch *mf);
  void force_tag_access(new_addr_type addr, unsigned time,
                        mem_access_sector_mask_t mask);
  void flush();
  void invalidate();
  void print(FILE *fp, unsigned &accesses, unsigned &misses) const;
  void display_state(FILE *fp) const;

 private:
  struct request {
    request() : token(0), mf(NULL), line(0), write(false), atomic(false) {}
    unsigned token;
    mem_fetch *mf;
    new_addr_type line;
    bool write;
    bool atomic;
  };

  struct line_state {
    line_state() : dirty(false), last_touch(0) {}
    bool dirty;
    unsigned long long last_touch;
  };

  struct aad_entry {
    aad_entry() : lower_mf(NULL), lower_issued(false) {}
    std::vector<unsigned> tokens;
    mem_fetch *lower_mf;
    bool lower_issued;
  };

  struct scheduled_response {
    scheduled_response() : ready_time(0), token(0) {}
    scheduled_response(unsigned long long ready, unsigned t)
        : ready_time(ready), token(t) {}
    unsigned long long ready_time;
    unsigned token;
  };

  struct wbq_entry {
    wbq_entry() : line(0), mf(NULL), issued(false) {}
    new_addr_type line;
    mem_fetch *mf;
    bool issued;
  };

  bool fixed_mode() const;
  unsigned bank_for(new_addr_type line) const;
  bool line_hit(new_addr_type line) const;
  bool request_is_write(mem_fetch *mf) const;
  void process_tag(unsigned token, unsigned long long time);
  void install_line(new_addr_type line, unsigned long long time);
  bool victim_requires_wbq() const;
  void enqueue_writeback(new_addr_type line, unsigned long long time);
  void schedule_response(unsigned token, unsigned long long ready_time);
  void retire_ready_responses(unsigned long long time);
  void issue_lower_reads(unsigned long long time);
  void issue_writebacks(unsigned long long time);
  void assert_unique_state() const;

  std::string m_name;
  l2_cache_config &m_cache_config;
  const memory_config *m_memory_config;
  mem_fetch_interface *m_memport;
  mem_fetch_allocator *m_mf_allocator;
  unsigned m_next_token;

  std::map<unsigned, request> m_requests;
  std::map<mem_fetch *, unsigned> m_token_for_mf;
  std::deque<unsigned> m_tag_queue;
  std::map<new_addr_type, aad_entry> m_aad;
  std::deque<new_addr_type> m_lower_read_queue;
  std::map<mem_fetch *, new_addr_type> m_fill_waiters;
  std::map<new_addr_type, line_state> m_lines;
  std::deque<scheduled_response> m_scheduled_responses;
  std::deque<mem_fetch *> m_response_ready;
  std::deque<wbq_entry> m_wbq;
  std::set<mem_fetch *> m_writeback_mfs;
  std::vector<unsigned long long> m_bank_ready;

  unsigned long long m_accesses;
  unsigned long long m_hits;
  unsigned long long m_misses;
  unsigned long long m_aad_merges;
  unsigned long long m_otf_reads;
  unsigned long long m_writes;
  unsigned long long m_writebacks;
  unsigned long long m_atomic_requests;
  unsigned long long m_token_stalls;
  unsigned long long m_aad_stalls;
  unsigned long long m_bank_stalls;
};

#endif
