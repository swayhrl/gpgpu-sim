# C16 strong-baseline implementation contract V1

Status: `VERIFIED_CODE` for PRIORITY_ALL, PRIORITY_STABLE, and DRRIP;
`SHIP_SW_IMPLEMENTATION_CONTRACT_INCOMPLETE` for SHiP-SW-style.

## Common control

The master controls are `-gpgpu_l2_c16_baseline_enable`, integer policy
`0=NONE, 1=PRIORITY_ALL, 2=PRIORITY_STABLE, 3=DRRIP, 4=SHIP_SW`, and an
independent diagnostics switch. Source defaults are OFF/NONE/OFF. An enabled
NONE policy and a disabled non-NONE policy fail closed. Functional baseline
enable and oracle-elastic functional enable are mutually exclusive.

The implementation is restricted to the accepted 128-byte, sectorized,
allocate-on-miss L2. It never stalls an otherwise admissible request, searches
only the current set, and does not change physical capacity, MSHRs, queues,
write policy, or request ordering. Diagnostics do not enable functionality.

## PRIORITY_ALL

The accepted oracle interval parser and request eligibility identify exactly
the same qweight global-read targets. Every target new-line allocation becomes
priority. Invalid selection preserves the baseline last-invalid rule. With no
invalid line, the baseline LRU/FIFO timestamp chooses among ordinary eligible
victims when any exist, otherwise among priority eligible victims. There is no
quota, pending-quota accounting, class floor, timer, cross-set victim, or
promotion/demotion.

## PRIORITY_STABLE

This is the same priority substrate, but priority eligibility additionally
requires `C16_M1F_STABLE_ADMISSION_HASH_V1` with seed
`0x6a09e667f3bcc908`, threshold `0x0484baf3b723b966`, and key
`(target_class << 32) xor region_relative_128B_line_index`. Unselected targets
remain ordinary. The 28-region/265,216-lines-per-region geometry is checked at
startup. No seed, threshold, quota, or balancing is added.

Priority resident and pending occupancy, their peaks, and each L2
subpartition's counters are reported. These are actual policy occupancies, not
a B16 guarantee.

## 2-bit DRRIP-HP

RRPV is stored at the 128-byte victim-line granularity. Accepted new-line
allocations insert with SRRIP RRPV=2 or deterministic BRRIP RRPV=3 except one
in 32 at RRPV=2. A true valid-sector demand hit promotes to zero. Victim search
uses the first eligible RRPV=3 way in fixed way order; otherwise all eligible
RRPVs age with saturation and search retries. No LRU tie-break is added.

Each L2 subpartition has a 10-bit PSEL initialized to 512 and a deterministic
BRRIP miss counter initialized to zero. For 2,048 sets, anchors `slot*32` for
slots 0..63 alternate between 32 SRRIP and 32 BRRIP leaders. More generally,
anchor `floor(slot*nsets/64)` is used. SRRIP-leader accepted misses increment
PSEL, BRRIP-leader accepted misses decrement it; followers use BRRIP at
PSEL>=512.

## SHiP-SW-style boundary

Policy ID 4 exists so configs and future merge contracts are stable, but it
fails closed. LR03/LR05 close original SHiP training but do not uniquely close
the software-region signature granularity, finite SHCT mapping/collision,
sampling, and C16 software-marking contract. No guessed predictor is present.
