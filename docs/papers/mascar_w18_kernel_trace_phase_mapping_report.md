# Mascar W18 Kernel Trace Phase Mapping Report

W18 implemented and ran default-off kernel launch tracing to improve Table III phase mapping for W17 app-level-ready rows.

Results:

- simulator_source_modified: yes
- traced_workloads: 9
- kernel_trace_rows: 60
- begin_rows: 30
- end_rows: 30
- phase_rows_mapped_by_confidence: inferred_order=9;not_w18_target=21
- unresolved_rows: 0

Conclusion: W18 produced a proposed 30-row manifest. The 9 W17 phase-pending rows now have local launch index/name evidence, but confidence is inferred_order rather than exact, because mapping uses paper suffix/order against local launch order and not an independent paper-to-source phase annotation.

Do not overwrite canonical manifests until GPT review accepts the proposed mappings.
