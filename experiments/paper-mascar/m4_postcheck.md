start_ts=1780568785
end_ts=1780569263
elapsed_sec=478
start_iso=2026-06-04T18:26:25+08:00
end_iso=2026-06-04T18:34:23+08:00

Validation
- git diff --check: pass
- source setup_environment release && make -j2: pass
- M4 symbol grep: experiments/paper-mascar/m4_symbol_grep.txt
- M4 diff name-status: experiments/paper-mascar/m4_diff_name_status.txt
- M4A config active reexec: off
- M4B config active reexec: on
- old proxy scheduling in M4 configs: off
- smoke: skipped; no self-contained short runner obvious in repo and full benchmarks are disallowed
