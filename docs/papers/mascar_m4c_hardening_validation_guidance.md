# Mascar M4C Detailed Guidance: Hardening, Validation, Documentation, and Review Pack

## Stage position

This is M4C, the closeout stage of the M4 big round.

M4S preflighted M2/M3.
M4A added the queue skeleton/passive stats.
M4B activated load-only re-execution.
M4C hardens the implementation, runs checks, writes docs, and packages the review files.

## Required hardening checks

### A. Behavior gating

Confirm all active behavior is gated by:

  gpgpu_enable_mascar
  gpgpu_mascar_enable_reexec_queue

Confirm defaults are off:
  gpgpu_mascar_enable_reexec_queue_probe 0
  gpgpu_mascar_enable_reexec_queue 0

Confirm baseline safety:
  gpgpu_enable_mascar=0 should not execute M4 queue behavior.

### B. No lower-level request for non-owner MP miss

Audit active retry path and initial path.

For non-owner in MP:
  L1 HIT may complete
  MISS / SECTOR_MISS / HIT_RESERVED / RESERVATION_FAIL must not send L2 request
  entry should NACK/requeue if active queue
  or local retry if queue cannot accept

Use grep and code comments to make this clear.

### C. mem_fetch ownership audit

Document each case:

Initial enqueue success:
  queue owns mf

Initial enqueue failure:
  existing normal path owns/deletes mf

Retry HIT:
  queue completes and deletes mf

Retry MISS/HIT_RESERVED accepted:
  cache/memory owns mf

Retry RESERVATION_FAIL/NACK:
  queue keeps mf and requeues

Latency queue enqueue:
  l1_latency_queue slot becomes NULL only after queue owns mf

No-latency enqueue:
  accessq is popped only after queue owns mf

### D. Scoreboard audit

Confirm re-exec HIT releases load dependency correctly.

Check:
  m_pending_writes decrement
  m_scoreboard->releaseRegister
  m_core->warp_inst_complete
  LDGSTS dependency handling if applicable

### E. Queue full / deadlock guard

Confirm:
  queue occupancy max printed
  queue full stalls new requests
  reexec retry continues
  repeated NACK/rotation is counted
  owner takeover from head can happen when MP has no owner

Do not add complicated deadlock logic unless needed by build/smoke.
Prefer simple counters and owner takeover.

## Documentation

Create:

  docs/papers/mascar_m4_status.md

Required sections:

1. Current implemented mechanism
2. M1 L1 saturation
3. M2 EP/MP owner scheduling
4. M3 non-owner hit-only/NACK
5. M4 load-only re-execution queue
6. What still differs from the paper
7. Store/atomic/texture limitations
8. Validation summary
9. Recommended M5 experiments

Also update or create:

  docs/papers/mascar_m4_known_limitations.md

Limitations to document:

- Current repository is GPGPU-Sim 4.x, not paper's v3.2.2.
- Current configs may be SM7/QV100, not paper's Fermi GTX480.
- M4 active re-exec is load-only by default.
- Non-owner stores/atomics are still blocked by scheduler.
- Texture/constant saturation is not fully modeled unless already covered by existing probes.
- Re-exec queue entry stores mem_fetch pointer rather than paper's compact 301-bit metadata.
- One-entry-per-warp is conservative and may serialize coalesced requests more than paper.
- Smoke may be skipped if benchmark env missing.

## Postcheck

Create:

  experiments/paper-mascar/m4_postcheck.md
  experiments/paper-mascar/m4_diff_name_status.txt
  experiments/paper-mascar/m4_symbol_grep.txt

Postcheck must include:

- start_iso
- end_iso
- elapsed_sec using start_ts/end_ts
- branch and HEAD
- git status before and after
- M4S smoke/preflight summary
- M4A summary
- M4B summary
- M4C hardening summary
- build commands and result
- smoke commands and result, or reason skipped
- changed files
- config checks
- grep checks
- review pack path
- warnings and limitations

## Required validation commands

Run:

  git diff --check
  source setup_environment release && make -j2

Run grep checks and save to m4_symbol_grep.txt:

  grep -RIn "gpgpu_mascar_enable_reexec_queue\\|gpgpu_mascar_reexec_queue_size\\|paper_mascar_m4_" src/gpgpu-sim configs docs experiments

Run config checks manually or by grep:
  M4A probe config has reexec_queue 0
  M4B load config has reexec_queue 1
  old proxy scheduling off in both

Smoke:
  If an obvious smoke command was found in M4S, run one tiny M4B smoke after M4B.
  If smoke fails, debug/fix.
  If benchmark env is missing, skip and document.

Do not run full benchmark suites.

## Review pack

Create:

  /workspace/tmp/mascar_m4_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include all changed source/config/docs/postcheck/helper files, including:

  src/gpgpu-sim/gpu-cache.cc
  src/gpgpu-sim/gpu-cache.h
  src/gpgpu-sim/gpu-sim.cc
  src/gpgpu-sim/shader.cc
  src/gpgpu-sim/shader.h
  configs/hrl-repro/SM7_QV100_mascar_m4_reexec_probe_on/
  configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/
  docs/papers/mascar_m4a_reexec_queue_skeleton.md
  docs/papers/mascar_m4b_active_load_reexec.md
  docs/papers/mascar_m4_status.md
  docs/papers/mascar_m4_known_limitations.md
  experiments/paper-mascar/m4s_smoke_notes.md
  experiments/paper-mascar/m4_postcheck.md
  experiments/paper-mascar/m4_diff_name_status.txt
  experiments/paper-mascar/m4_symbol_grep.txt

Do not commit M4 implementation outputs.

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. build/smoke status
5. files GPT should review
