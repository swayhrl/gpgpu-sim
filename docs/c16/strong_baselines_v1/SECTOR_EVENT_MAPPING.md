# Sector-L2 event mapping

The production path was mapped at Core `0271de82432db004beed43280ed01057246a0f2c`.

| GPU L2 event | Production evidence | Baseline update |
|---|---|---|
| True valid same-sector hit | tag matches and requested sector is VALID/MODIFIED; accepted handler calls `tag_array::access` once | DRRIP HP promotes line RRPV to 0; trainable demand only |
| Same-tag different-sector first touch | tag matches, requested sector INVALID while another sector is valid, yielding `SECTOR_MISS` | allocate sector only; no RRPV promotion, insertion, PSEL, or BRRIP event |
| Accepted new-line miss | initial probe is MISS; resource/MSHR/MissQ checks pass; committed `tag_array::access` allocates the line | exactly one victim decision, insertion, leader/PSEL event, and deterministic BRRIP counter event |
| MSHR merge | `send_read_request` sees existing MSHR and committed access returns `HIT_RESERVED` | no promotion, insertion, PSEL, or training |
| Retry/reservation fail | resource failure returns before committed tag-array access, or all ways are reserved | no metadata update; later accepted retry produces the sole update |
| Fill | `tag_array::fill` changes sector RESERVED to VALID/MODIFIED | commits priority pending to resident; no DRRIP promotion/insertion/training |
| Writeback/internal write-allocate hit | access types L1/L2 writeback or internal write-allocate | no DRRIP hit promotion; allocations still receive replacement metadata |
| Atomic demand | ordinary GLOBAL demand with atomic flag | a true valid-sector hit is a real re-reference and promotes once |
| PTE/synthetic/special demand read | distinct demand access type, not writeback | true valid-sector hit promotes once; miss inserts normally |

RRPV granularity is the same `cache_block_t`/128-byte line chosen as victim.
Sector child status remains separate. Probe-mode victim calculation uses a
copy: aging/PSEL/counters mutate only at the committed access after resources
admit the request.
