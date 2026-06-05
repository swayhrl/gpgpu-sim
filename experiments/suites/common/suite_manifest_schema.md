# Full-Suite Manifest Schema

Canonical columns:

- `suite_id`: stable suite key, for example `rodinia` or `parboil`.
- `suite_name`: human-readable suite name.
- `app_id`: stable workload key unique within this framework.
- `app_name`: human-readable app name.
- `benchmark_name`: upstream benchmark directory/name.
- `variant`: build/input variant, or `default`.
- `source_path`: local source directory or `unknown`.
- `binary_path`: local executable path or `unknown`.
- `data_path`: local data/input path or `unknown`.
- `build_command`: known build command or `unknown`.
- `run_command`: command used by the runner or `unknown`.
- `wrapper_path`: stable wrapper path or generator note.
- `wrapper_status`: `ready` or placeholder/unavailable status.
- `availability_status`: taxonomy status describing local availability.
- `build_status`: build evidence status.
- `data_status`: data availability status.
- `command_status`: command verification status.
- `smoke_status`: last smoke classification or `not_run`.
- `correctness_status`: `explicit_pass`, `no_explicit_pass`, `failed`, or `unknown`.
- `input_scale`: `tiny`, `small`, `unknown`, etc.
- `default_timeout_sec`: default timeout for actual runs.
- `current_ready`: `1` only for W20 current baseline rows.
- `table_iii_related`: `1` if related to Mascar Table III naming.
- `table_iii_paper_ids`: semicolon-separated related Table III IDs.
- `phase_mapping_status`: phase status if known, otherwise `not_applicable` or `unknown`.
- `blocker`: concise blocker for non-ready rows.
- `next_action`: next practical action.
- `notes`: evidence and caveats.

Rows must never be dropped solely because they are unavailable. Use `unknown` rather than blank when a field cannot be determined.
