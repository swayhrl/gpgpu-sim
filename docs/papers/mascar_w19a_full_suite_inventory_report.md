# Mascar W19A Full-Suite Inventory Audit

start_ts=1780642358
end_ts=1780643776
elapsed_sec=1418

W19A audited local Rodinia CUDA and Parboil benchmark source trees while preserving W18 outputs as read-only background.

Outputs:

- experiments/suites/common/rodinia_full_manifest.csv
- experiments/suites/common/parboil_full_manifest.csv

Inventory counts:

- Rodinia rows: 23
- Parboil rows: 18
- Rodinia existing ready rows: 8
- Parboil existing ready rows: 5
- Non-ready blocker summary: binary_available_command_unverified:8;source_available_missing_binary:20

W18 context retained and not overwritten.
