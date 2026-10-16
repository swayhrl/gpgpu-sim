// Lane 4 diagnostic reference memory. Not physical cache/payload allocation.
#ifndef EP_L2_IDEAL_REFERENCE_H
#define EP_L2_IDEAL_REFERENCE_H
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace ep_l2_ideal {
struct Byte {
  uint64_t address;
  uint64_t version;
  bool value_known;
  uint8_t value;
  bool operator==(const Byte &b) const {
    return address == b.address && version == b.version &&
           value_known == b.value_known && (!value_known || value == b.value);
  }
};
// SASS traces have no data bytes/initial memory image. Their explicit
// functional reference is (byte address,last committed producer version).
// Cold data is a typed symbol, never an uninitialized cache-line value.
// Production-path tests seed bytes and known writes through this same store.
class ReferenceMemory {
 public:
  typedef std::array<bool, 128> Mask;
  typedef std::array<uint8_t, 128> Data;
  void seed(uint64_t address, const std::vector<uint8_t> &bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) known_[address + i] = bytes[i];
  }
  void write(uint64_t line, const Mask &mask, const Data *data = nullptr) {
    assert((line & 127) == 0 && next_version_ != std::numeric_limits<uint64_t>::max());
    const uint64_t version = ++next_version_;
    auto it = versions_.find(line);
    if (it == versions_.end()) it = versions_.emplace(line, Versions{}).first;
    for (unsigned b = 0; b < 128; ++b) if (mask[b]) {
      it->second[b] = version;
      if (data) known_[line + b] = (*data)[b];
      else known_.erase(line + b);
    }
  }
  // Successful host prefill replaces selected sectors with fresh unknown
  // version-zero symbols, matching the FT Payload contract. Never reset
  // the producer clock or modify unselected bytes/older response copies.
  void host_prefill(uint64_t line, const Mask &mask) {
    assert((line & 127) == 0);
    auto it = versions_.find(line);
    for (unsigned b = 0; b < 128; ++b) if (mask[b]) {
      if (it != versions_.end()) it->second[b] = 0;
      known_.erase(line + b);
    }
  }
  std::vector<Byte> read(uint64_t line, const Mask &mask) const {
    assert((line & 127) == 0);
    std::vector<Byte> out;
    const auto v = versions_.find(line);
    for (unsigned b = 0; b < 128; ++b) if (mask[b]) {
      const auto k = known_.find(line + b);
      out.push_back(Byte{line + b, v == versions_.end() ? 0 : v->second[b],
                         k != known_.end(), k == known_.end() ? uint8_t(0) : k->second});
    }
    return out;
  }
 private:
  typedef std::array<uint64_t, 128> Versions;
  uint64_t next_version_ = 0;
  std::map<uint64_t, Versions> versions_;
  std::map<uint64_t, uint8_t> known_;
};
}  // namespace ep_l2_ideal
#endif
