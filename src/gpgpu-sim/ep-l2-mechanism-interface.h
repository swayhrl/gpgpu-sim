#ifndef EP_L2_MECHANISM_INTERFACE_H
#define EP_L2_MECHANISM_INTERFACE_H

#include "ep-l2-ft-interface.h"

// Mechanism study 2026-10-19, interface revision 1. These are identity and
// budget declarations, not another allocator, address index, or data array.
namespace ep_l2 {
namespace mechanism_v1 {
static const unsigned k_interface_revision = 1;
static const unsigned k_tags = 1024;
static const unsigned k_payloads = 1168;
static const unsigned k_line_bytes = 128;
static const unsigned k_sectors = 4;
static const unsigned k_banks = 4;
static const unsigned k_wad_and_victim_entries = 128;
static const unsigned k_line_mshrs = 128;
static const unsigned k_request_descriptors = 512;
static const unsigned k_victim_query_cycles = 1;
static const unsigned k_victim_queries_per_cycle = 1;

enum LocalDiagnostic { NORMAL = 0, COPY_FREE = 1, L2_DATA_FREE = 2 };
enum WaitReason {
  WB_DATA_SPACE = 0,
  BANK_COMPATIBLE_LANDING,
  TAG_SET_PENDING,
  LINE_MSHR_FULL,
  DESCRIPTOR_FULL,
  PER_LINE_CAP,
  WAD_FULL,
  SAME_ADDRESS_WB,
  PINNED_PAYLOAD,
  FRONTEND_TRANSACTION,
  MISS_QUEUE_FULL,
  RESPONSE_QUEUE_FULL,
  DATA_SERVICE,
  COPY_PIPELINE,
  // 14 is the published MSV1 any-to-admission output row, not a new wait.
  // Keep that identity stable when adding mechanism-specific measurements.
  VICTIM_LOOKUP = 15,
  WAIT_REASONS
};
static const unsigned k_legacy_any_to_admission_reason = 14;

// Every adapter must revalidate this generation and its own sector/owner
// premises. A Tag index alone cannot identify a detached or late response.
struct RequestIdentity {
  unsigned request_uid;
  PayloadHandle payload;
  unsigned tag_index;
  unsigned descriptor_id;
  unsigned sector_mask;
  RequestIdentity()
      : request_uid(0), tag_index((unsigned)-1),
        descriptor_id((unsigned)-1), sector_mask(0) {}
};
}  // namespace mechanism_v1
}  // namespace ep_l2
#endif
