#include "../oracle_elastic_residency.h"

#include <stdint.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using oracle_elastic_residency::interval;
using oracle_elastic_residency::oracle_config;

namespace {

void write_u64_le(std::ofstream *output, uint64_t value) {
  unsigned char bytes[8];
  for (unsigned i = 0; i < 8; ++i)
    bytes[i] = static_cast<unsigned char>((value >> (i * 8)) & 0xff);
  output->write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
}

std::string hex64(uint64_t value) {
  std::ostringstream output;
  output << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
  return output.str();
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "usage: m1f_static_selector SIDECAR SHA256 SELECTED.u64 SUMMARY.json\n";
    return 2;
  }
  try {
    const char *sidecar = argv[1];
    const char *accepted_sha = argv[2];
    const char *selected_path = argv[3];
    const char *summary_path = argv[4];
    oracle_config config;
    config.configure(true, 0, 131072, 128, 16, sidecar, accepted_sha, false,
                     true);
    std::ofstream selected(selected_path,
                           std::ios::out | std::ios::binary | std::ios::trunc);
    if (!selected) throw std::runtime_error("cannot create selected output");
    std::vector<uint64_t> per_class(29, 0);
    uint64_t total_lines = 0;
    uint64_t selected_lines = 0;
    const std::vector<interval> &intervals = config.intervals();
    for (size_t i = 0; i < intervals.size(); ++i) {
      const interval &item = intervals[i];
      for (uint64_t address = item.begin; address < item.end; address += 128) {
        ++total_lines;
        unsigned target_class = 0;
        uint64_t line_index = ~uint64_t(0);
        uint64_t hash = 0;
        const bool eligible = config.protection_eligible(
            address, &target_class, &line_index, &hash);
        if (target_class != item.target_class ||
            line_index != (address - item.begin) / 128)
          throw std::runtime_error("selector identity mismatch");
        if (eligible) {
          write_u64_le(&selected, address);
          ++per_class[target_class];
          ++selected_lines;
        }
      }
    }
    selected.close();
    if (!selected) throw std::runtime_error("selected output write failure");
    std::ofstream summary(summary_path, std::ios::out | std::ios::trunc);
    if (!summary) throw std::runtime_error("cannot create summary output");
    summary << "{\n"
            << "  \"schema\": \"C16_E1_M1F_STATIC_SELECTOR_V1\",\n"
            << "  \"status\": \"PASS\",\n"
            << "  \"hash_version\": \""
            << oracle_elastic_residency::m1f_hash_version() << "\",\n"
            << "  \"seed_hex\": \""
            << hex64(oracle_elastic_residency::m1f_hash_seed()) << "\",\n"
            << "  \"threshold_hex\": \""
            << hex64(config.m1f_threshold()) << "\",\n"
            << "  \"global_quota_lines\": 131072,\n"
            << "  \"family_lines\": 7426048,\n"
            << "  \"total_lines\": " << total_lines << ",\n"
            << "  \"selected_lines\": " << selected_lines << ",\n"
            << "  \"classes\": [\n";
    for (unsigned target_class = 1; target_class <= 28; ++target_class) {
      summary << "    {\"target_class\": " << target_class
              << ", \"selected_lines\": " << per_class[target_class]
              << "}" << (target_class == 28 ? "\n" : ",\n");
    }
    summary << "  ]\n}\n";
    summary.close();
    if (!summary) throw std::runtime_error("summary output write failure");
    std::cout << "M1F_STATIC_SELECTOR_PASS total=" << total_lines
              << " selected=" << selected_lines << "\n";
  } catch (const std::exception &error) {
    std::cerr << "M1F_STATIC_SELECTOR_FAIL: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
