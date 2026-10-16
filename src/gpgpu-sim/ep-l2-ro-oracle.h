#ifndef EP_L2_RO_ORACLE_H
#define EP_L2_RO_ORACLE_H
#include <algorithm>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ep_l2_ro {
// Experimental, shared read-only input. The launch preflight verifies the
// complete manifest/artifact/trace hashes before handing these bytes to Core.
// This parser validates the range grammar, not cryptographic hashes itself.
class oracle_certificate {
 public:
  oracle_certificate(std::istream &verified_ranges,
                     const std::string &verified_manifest_sha256,
                     const std::string &scope)
      : m_manifest(verified_manifest_sha256) {
    if (scope != "TRACE_MODEL_CLOSED_WORLD" || m_manifest.size() != 64 ||
        m_manifest.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw std::invalid_argument("unverified oracle scope/identity");
    std::string line;
    if (!std::getline(verified_ranges,line))
      throw std::invalid_argument("empty oracle input");
    if (!line.empty() && line.back()=='\r') line.pop_back();
    if (line != "first_line,last_line_inclusive")
      throw std::invalid_argument("unsupported oracle range grammar");
    while (std::getline(verified_ranges,line)) {
      if (!line.empty() && line.back()=='\r') line.pop_back();
      if (line.size()!=33 || line[16]!=',' ||
          line.substr(0,16).find_first_not_of("0123456789abcdef")!=std::string::npos ||
          line.substr(17).find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("corrupt oracle range");
      uint64_t first=std::stoull(line.substr(0,16),NULL,16);
      uint64_t last=std::stoull(line.substr(17),NULL,16);
      if (first%128 || last%128 || first>last ||
          (!m_ranges.empty() && first<=m_ranges.back().second))
        throw std::invalid_argument("unaligned/overlapping oracle range");
      m_ranges.push_back(std::make_pair(first,last));
    }
    if (!verified_ranges.eof()) throw std::invalid_argument("incomplete oracle read");
  }
  bool contains(uint64_t line) const {
    if (line%128) return false;
    auto it=std::upper_bound(m_ranges.begin(),m_ranges.end(),
                            std::make_pair(line,~uint64_t(0)));
    if (it==m_ranges.begin()) return false;
    --it;return line<=it->second;
  }
  const std::string &manifest_sha256() const { return m_manifest; }
  unsigned range_count() const { return m_ranges.size(); }
 private:
  std::string m_manifest;
  std::vector<std::pair<uint64_t,uint64_t> > m_ranges;
};
} // namespace ep_l2_ro
#endif
