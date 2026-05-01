# SM7_QV100_mascar_policy_on

Mascar Phase 4 minimal scheduling policy config.
gpgpu_mascar_enable_scheduling=1 — real scheduler skip active.
stall_threshold=2, max_skip_streak=4 (deadlock prevention).
Skip gate placed post-scoreboard inside memory instruction block.
