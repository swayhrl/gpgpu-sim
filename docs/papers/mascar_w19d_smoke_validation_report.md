# Mascar W19D Smoke Validation Report

start_ts=1780642358
end_ts=1780643776
elapsed_sec=1418

W19D ran actual smoke on all W19C eligible ready/build-success rows.

Outputs:

- experiments/suites/common/w19_smoke_results.csv
- experiments/suites/common/w19_smoke_summary.md
- experiments/suites/common/w19_smoke_run_manifest.csv
- experiments/suites/common/w19_smoke_status_matrix.csv

Smoke result:

- smoke_pass: 13
- smoke_fail: 4
- passed_workloads: rodinia_backprop rodinia_bfs rodinia_hotspot rodinia_kmeans rodinia_lud rodinia_nw rodinia_pathfinder rodinia_srad parboil_histo parboil_mri_q parboil_sgemm parboil_spmv parboil_stencil
- failed_build_recovered_candidates: rodinia_gaussian rodinia_myocyte rodinia_nn rodinia_streamcluster

Conclusion: no build-recovered candidate is promoted to ready in W19D; final smoke-ready count remains 13.
