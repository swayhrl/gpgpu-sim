# W17C Postcheck

start_ts=1780638310
end_ts=1780638774
elapsed_sec=464

## Checks

- w17_command_manifest_updated.csv has 30 rows.
- w17c_smoke_new_wrappers.sh passed bash -n.
- dry-run smoke covered all 30 wrappers.
- actual smoke ran 9 app-level promoted rows with timeout.

## Result

Dry-run: 30/30 pass. Actual app-level smoke: 9/9 pass.
