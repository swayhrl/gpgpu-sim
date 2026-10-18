#ifndef DTC_L1_TAG_INDEX_H
#define DTC_L1_TAG_INDEX_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "../option_parser.h"

namespace dtc_l1 {

inline void register_tag_hash_bankrow_option(option_parser_t parser,
                                             bool *enabled) {
  option_parser_register(parser, "-gpgpu_dtc_l1_tag_hash_bankrow", OPT_BOOL,
                         enabled,
                         "Opt-in fixed bank-preserving XOR Tag row index; "
                         "PAPER_OO whole-line finite V1 only", "0");
}

inline void check_tag_hash_bankrow(bool enabled, bool paper_oo, bool finite,
                                   unsigned sets, unsigned banks) {
  if (!enabled) return;  // K0 keeps every old geometry and recency rule.
  const char *reason = nullptr;
  if (!paper_oo)
    reason = "TAG_HASH_BANKROW_PAPER_OO_ONLY_STOP";
  else if (!finite)
    reason = "TAG_HASH_BANKROW_FINITE_V1_REQUIRED_STOP";
  else if (banks != 4 || sets < 4 || (sets & (sets - 1)) || sets % 4)
    reason = "TAG_HASH_BANKROW_UNSUPPORTED_GEOMETRY_STOP";
  if (reason) {
    std::fprintf(stderr, "%s: sets=%u banks=%u\n", reason, sets, banks);
    std::abort();
  }
}

// Constructor validation guarantees the ON geometry. The shifts/mask are
// fixed wiring for a configured S=2^s; row selection uses s-2 parallel XORs.
// Complete logical line addresses remain stored/compared by the caller.
inline unsigned logical_tag_set(uint64_t address, unsigned sets, bool hash) {
  if (!hash) return static_cast<unsigned>((address / 128) % sets);
  unsigned s = 0;
  for (unsigned n = sets; n > 1; n >>= 1) ++s;
  const uint64_t line = address >> 7;
  const unsigned row = static_cast<unsigned>(
      ((line >> 2) ^ (line >> s)) & ((sets / 4) - 1));
  return 4 * row + static_cast<unsigned>(line & 3);
}

}  // namespace dtc_l1
#endif
