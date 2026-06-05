# Mascar W18A Existing Kernel Log Audit

start_ts=1780640595
end_ts=1780641157
elapsed_sec=562

W18A audited existing simulator/source log paths and W17 phase-pending rows before adding new trace support.

Outputs:
- experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- experiments/paper-mascar/workloads/matrix/W18/w18a_existing_log_trace_audit.py
- experiments/paper-mascar/workloads/matrix/W18/w18a_phase_pending_targets.csv
- experiments/paper-mascar/workloads/audit/W18/w18a_kernel_source_grep.txt
- experiments/paper-mascar/workloads/audit/W18/w18a_existing_log_grep.txt

Finding: existing logs did not provide a reliable structured begin/end kernel trace for the W17 app_level_pending_kernel_trace rows, so W18B default-off simulator trace was required.
