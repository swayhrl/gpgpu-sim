# Mascar M3 Status

## Implemented

- M1: passive L1 saturation probe, used as the L1-side saturation signal for
  later Mascar stages.
- M2: EP/MP mode tracking, shared per-SM owner state, compute-first MP ordering,
  and owner-warp scheduler gating.
- M3A: passive non-owner L1 hit-only would-hit / would-NACK probe.
- M3B: active non-owner load hit-only access in MP mode, local miss/reserved
  NACK, both no-latency and L1 latency queue paths, and a NACK owner-release
  guard.

## Active M3 Boundaries

Active M3 behavior requires:

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=1`
- `gpgpu_mascar_enable_nonowner_hit_only=1`
- MP mode and a valid owner
- non-owner load candidate
- L1D path

Non-owner stores, atomics, and non-load memory instructions remain blocked by
the M2 scheduler gate.

## Remaining M4 Work

M4 still needs the paper's re-execution queue, request recycling queue, and
one-memory-instruction-per-warp re-execution queue enforcement. M3B intentionally
does not recycle NACKed requests through a separate queue.
