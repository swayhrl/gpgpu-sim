# Mascar M4 Known Limitations

- Re-execution is load-only. Store, atomic, texture, and constant paths are not
  supported by the queue.
- M4 does not add a separate request recycling queue beyond the LSU-owned
  re-execution queue.
- M4 keeps one queued entry per warp, but it does not implement a broader paper
  validation matrix; M5 should cover focused workloads and stats sanity.
- The NACK rotation guard records repeated rotations and resets the local streak;
  broader recovery still relies on M2/M3 owner release guards.
- Workload smoke was skipped in M4 because no obvious short runner was available
  inside the repo and full benchmarks are disallowed.

