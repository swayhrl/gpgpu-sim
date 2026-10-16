#ifndef EP_L2_FT_INTERFACE_H
#define EP_L2_FT_INTERFACE_H

// Shared identity ABI for FT20261016 lanes. A handle is never an address key.
// Unsigned software storage is not a physical metadata width estimate.
namespace ep_l2 {
struct PayloadHandle {
  unsigned payload_id;
  unsigned generation;
  PayloadHandle(unsigned id = (unsigned)-1, unsigned gen = 0)
      : payload_id(id), generation(gen) {}
  bool valid() const { return payload_id != (unsigned)-1; }
  bool operator==(const PayloadHandle &other) const {
    return payload_id == other.payload_id && generation == other.generation;
  }
  bool operator!=(const PayloadHandle &other) const { return !(*this == other); }
};
enum PayloadRole {
  FREE, RESIDENT, NORMAL_FILL, WB_STAGED, BYPASS, RO_ATTACHED, RO_DETACHED,
  TVD_CLEAN, TVD_DIRTY_PENDING, TVD_DIRTY_INFLIGHT
};
enum Refusal { NONE, POOL_FULL, ROLE_QUOTA, BANK_CLASS_FULL, PINNED,
               WAD_FULL, LOWER_FULL, BANK_BUSY, STALE_HANDLE };
// Preview is immutable and carries no implicit reservation or data ownership.
struct Reservation {
  PayloadHandle landing;
  PayloadHandle victim;
  PayloadHandle staging;
  unsigned tag_index;
  Refusal refusal;
  bool valid;
  Reservation() : tag_index((unsigned)-1), refusal(NONE), valid(false) {}
};
}  // namespace ep_l2
#endif
