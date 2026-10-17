# Lane X opt-in invalidation V1: scientific STOP

Date: 2026-10-17 (Asia/Shanghai).

Status: `SCIENTIFIC_STOP_SOURCE_EVENT_CONTRACT_MISMATCH`.

Frozen executable source: `6fba0136f8465b59fe5858a950fa39efb265f654`.
This commit changes documentation only. No invalidation option or K1 behavior
has been implemented. Do not describe this branch or its independently built
baseline artifacts as a K1 implementation or a K1 correctness PASS.

The user's explicit STOP condition applies: the option description's purported
kernel-end event is not a once-per-kernel completion callback. In
`src/gpgpu-sim/gpu-sim.cc:2515-2527`, after CTA issue and latency decrement,
each CORE tick can invalidate every cluster whose `get_not_completed()==0`,
with `gpgpu_flush_l1_cache` enabled. Other clusters need not have finished;
the same idle cluster can be visited again on subsequent ticks. This is a
static source finding, not a measured claim about Gaussian/GEMM execution.

Additional callers are cache preference change (`gpu-sim.cc:1749-1754`,
independent of the flush option) and MEMBAR release
(`shader.cc:6877-6888`, gated by the existing flush option and that warp's
scoreboard). They do not establish a globally drained DTC lifecycle.

Furthermore, `ldst_unit::invalidate()` (`shader.cc:4012-4015`) calls
`baseline_cache::invalidate()` (`gpu-cache.cc:1268-1279`), which records a
pending request. Actual conventional Tag invalidation occurs only in
`retire_pending_invalidate()` (`gpu-cache.cc:1281-1286`) once the miss queue,
extra fill owners and MSHRs have drained (`gpu-cache.h:1474-1478`). Thus an
invalidation request and its conventional Tag transition can occur at different
times. Their timing must not be conflated in a new DTC interface.

No guessed latency, extra drain, assertion relaxation, global reset, cache
change, or kernel-boundary rewrite is introduced to resolve these distinctions.
IO passive release has no general ownerless-invalidation interface; it must not
be replaced by OO references or an invented retiring instruction.

Review evidence is maintained on Framework branch
`hrl/iscas2027-dtc-lane-x-implementation-174-v1` under
`docs/dtc_l1/review_packs/DTC_LANE_X_OPTIN_INVALIDATION_V1/`.
External Lane V source-event contract and Lane Y independent validation are
mandatory before any later formal run. This window dispatches no full
Gaussian/GEMM simulation. A later implementation requires a resolved event
contract; this STOP does not authorize an alternative mechanism.
