# DRRIP GPU adaptation

This is a C16 adaptation of 2-bit DRRIP-HP, not an assertion that the CPU
paper specified GPU MSHR/sector behavior. The original insertion, hit,
victim-aging, set-dueling, 32+32 leader, 10-bit PSEL, and epsilon=1/32 rules
are preserved. The adaptation filters false temporal locality using the
production event map in `SECTOR_EVENT_MAPPING.md`.

Metadata comprises two RRPV bits per 128-byte L2 line, one 10-bit PSEL and one
deterministic miss counter per subpartition, plus diagnostics when requested.
Leader anchors are frozen before performance data. No host RNG, kernel UID,
target class, oracle label, M1 quota, or future information enters DRRIP.
