# Postcheck template

start_ts=$(date +%s)
end_ts=$(date +%s)
elapsed_sec=$((end_ts-start_ts))
branch=<branch>
HEAD=<head>

## Validation

- git diff --check: <status>
- syntax validation: <status>
- raw log policy: <status>
