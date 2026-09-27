# Historical L2 infrastructure reuse audit

Read-only branches inspected:

- `hrl/ep-l2-target-baseline-v0` at `0cde333340792cffed869cbbc7e7dc88667c6b8b`;
- `hrl/l2-frc-baseline-v1` at `97eb1e8301d410c8d720e6780fba40751e38000e`.

No commit or whole-file implementation was merged. Reused experience only:

- separate new-MSHR, merge, MissQ, data-port, and response-path admission
  failures;
- one line identity with independent pending/valid sector bits;
- stale fill/generation and wrong-sector checks as testing patterns;
- observation counters must not become functional predicates.

The historical branches have different ancestry and broad VM/L2 changes, so
their payload/FRC implementations are not suitable drop-in replacement code.
