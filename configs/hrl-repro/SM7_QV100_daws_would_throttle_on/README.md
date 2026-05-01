# SM7_QV100_daws_would_throttle_on

DAWS would-throttle telemetry config.
gpgpu_enable_daws=1, telemetry=1, would_throttle=1, throttling=0.
footprint_threshold=32 (warp_size; fires when sum(diverged_threads) > 32).
No scheduling behavior change.
