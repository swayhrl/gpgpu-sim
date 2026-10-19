#ifndef DTC_SEM1_VALUES_H
#define DTC_SEM1_VALUES_H
// Isolated, compile-time default-OFF conditional test data plane. It has no
// reference-memory input. Explicit refill bytes enter only via real LDST replies;
// observed bytes are read from oo_frontend's diagnostic physical byte array.
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>
namespace dtc_l1 {
class sem1_values {
 public:
  using bytes32 = std::array<std::uint8_t, 32>;
  struct binding { unsigned physical; std::uint64_t generation; unsigned sector; };
  struct sample { binding source; bytes32 bytes; std::uint64_t cycle; };
  struct consumer {
    std::vector<binding> references;
    std::vector<sample> collected;
    std::uint64_t retire_cycle = UINT64_MAX;
  };
  // Fixture transport attaches bytes to a real reply mem_fetch's unique UID.
  void inject_reply(unsigned reply_uid, const bytes32 &bytes) {
    need(packets_.emplace(reply_uid, bytes).second, "duplicate fixture packet");
  }
  bytes32 accept_reply(unsigned reply_uid) {
    auto p = packets_.find(reply_uid);
    need(p != packets_.end(), "production reply missing explicit test payload");
    auto bytes = p->second; packets_.erase(p); ++accepted_packets; return bytes;
  }
  // Called at successful real access(): no speculative or rejected association.
  void bind(std::uint64_t uid, unsigned physical, std::uint64_t generation,
            unsigned requested_sector_mask) {
    need(requested_sector_mask && !(requested_sector_mask & ~15U), "bad consumer sector mask");
    auto &c = consumers[uid];
    need(c.retire_cycle == UINT64_MAX, "UID reused after retirement");
    for (unsigned s=0;s<4;++s) if (requested_sector_mask & (1U<<s))
      c.references.push_back({physical,generation,s});
  }
  binding next(std::uint64_t uid) {
    auto it = consumers.find(uid);
    need(it != consumers.end(), "collection without accepted consumer binding");
    auto &c = it->second;
    need(c.collected.size() < c.references.size(), "duplicate collection");
    return c.references[c.collected.size()];
  }
  void collected(std::uint64_t uid, binding b, const bytes32 &bytes,
                 std::uint64_t cycle) {
    const auto expected_route = next(uid);
    need(expected_route.physical==b.physical && expected_route.generation==b.generation &&
         expected_route.sector==b.sector, "collection cursor route changed");
    consumers.at(uid).collected.push_back({b,bytes,cycle});
  }
  void retired(std::uint64_t uid, std::uint64_t cycle) {
    auto it = consumers.find(uid);
    need(it!=consumers.end(), "retirement missing consumer"); auto &c=it->second;
    need(c.retire_cycle==UINT64_MAX && c.collected.size()==c.references.size(), "incomplete or duplicate consumer output");
    need(!c.collected.empty() && cycle>c.collected.back().cycle, "retire before collect+1");
    c.retire_cycle=cycle;
  }
  bool packets_empty() const { return packets_.empty(); }
  std::map<std::uint64_t,consumer> consumers;
  unsigned accepted_packets = 0;
 private:
  static void need(bool ok,const char *why) { if(!ok) throw std::logic_error(why); }
  std::map<unsigned,bytes32> packets_;
};
} // namespace dtc_l1
#endif
