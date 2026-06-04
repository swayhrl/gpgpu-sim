# Mascar M4 Status

M4 implements a load-only cache access re-execution queue layered on M1 L1
saturation, M2 EP/MP owner scheduling, and M3 non-owner hit-only/NACK.

## Current Behavior

- Baseline remains gated by `gpgpu_enable_mascar=0`.
- M4 active behavior is additionally gated by
  `gpgpu_mascar_enable_reexec_queue=1`.
- M4 supports global/local/param-local loads only by default.
- Non-owner MP retries use M3 hit-only/NACK and therefore do not send L2
  requests on miss.
- Owner, EP, or no-owner retries use normal L1D access and can send misses to
  lower memory.
- Queue capacity, retry issue rate, NACK rotation guard, and owner takeover are
  configurable.

## Validation

- Pre-M4 static build passed in M4S.
- M4 implementation build passed with:

```bash
git diff --check && source setup_environment release && make -j2
```

- Workload smoke was not run because no self-contained short runner was obvious
  in the repository and full benchmarks are disallowed.

