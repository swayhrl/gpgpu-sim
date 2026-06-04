# Mascar M8B Guidance: Focused Sweep Execution

## Stage

This is M8B.

Input: W8 run plan / workload manifest from M8A

Goal:
- Execute focused subset sweep
- Collect per-workload/config M2/M3/M4 counters
- Capture runtime logs
- Ensure dry-run and actual runs are recorded
- Debug wrapper/config/runtime issues if needed

Tasks:
1. Use W3 common runner (run_gpgpusim_matrix.sh)
2. Execute dry-run for all subset workloads
3. Execute actual runs for activation-ready subset
4. Collect stats with collector
5. Record:
   - run_manifest
   - results.csv
   - summary.md
   - status_matrix.csv
6. Update postcheck with elapsed_sec, branch/HEAD, git status, notes
