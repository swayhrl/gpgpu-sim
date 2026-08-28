# L2CHARV1 output schema

Core records use `L2CHARV1|TYPE|key=value...` without locale-dependent
formatting.  `SLICE` contains final occupancy percentiles and frontend
blocking data; `SLICE_DETAIL` contains causal, port, WB, and lifetime data;
`WINDOW` is one per slice per configured L2-cycle window; `INVARIANT` reports
bookkeeping status. Percentiles use nearest rank: the smallest occupancy whose
cumulative sample count is at least 50% or 95% of all samples.

Instrumentation is off by default. Enable it with `-gpgpu_l2_char_enable 1`;
the default window is 5000 L2 cache-cycle invocations.
