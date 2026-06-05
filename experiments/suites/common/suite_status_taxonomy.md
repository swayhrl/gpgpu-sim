# Full-Suite Status Taxonomy

This taxonomy is the W20 canonical status vocabulary for Rodinia/Parboil and future paper workload manifests.

`ready`, `command_verified`, and only rows with explicit smoke-pass evidence count as runnable baselines. `completed_no_explicit_pass` is simulator completion evidence, not correctness-pass evidence.

All placeholder, missing, failed, and unsupported rows stay in the manifest and are dry-run visible. They are not actual-run by default.

See `suite_status_taxonomy.csv` for machine-readable policy columns: dry-run eligibility, actual-run default, ready counting, and next action.
