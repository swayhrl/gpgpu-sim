# M5 Workload Manifest

The focused M5 matrix uses short workloads only. The current manifest includes
`rodinia_hotspot`, discovered in `/workspace/repos/gpgpu-workloads`, because it
completed quickly in M5A and produced L1D pressure and Mascar stats.

To add more workloads later, append rows to `m5_workload_manifest.csv` with:

- `workload_id`: stable workload name.
- `suite`: workload suite label.
- `type`: memory behavior category.
- `command`: command to run from `working_dir`.
- `working_dir`: directory where the command should execute.
- `expected_runtime`: expected wall-clock class, keeping each run under 20
  minutes.
- `status`: use `available` only if binaries/data exist locally.
- `notes`: input size and validation caveats.

The runner injects each config with `GPGPUSIM_CONFIG_OVERRIDE` and sets
`TIMEOUT_SECONDS`, so commands should normally call the workload wrapper rather
than hand-copying configs.
