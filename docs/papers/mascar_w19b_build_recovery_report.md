# Mascar W19B Build and Binary Recovery Report

start_ts=1780642358
end_ts=1780643776
elapsed_sec=1418

W19B ran two build/recovery rounds:

- round1 wrapper_make for existing wrapper directories.
- round2 raw source builds: Rodinia make/Makefile_nvidia/lowercase makefile and Parboil compile cuda/cuda_base.

Results:

- build_pass: 19
- build_fail: 38
- raw_build_logs: /workspace/tmp/mascar_w19_build_logs_20260605_145525

Key blockers:

- Parboil driver invokes python but only python3 is available as python3.
- Several Rodinia legacy Makefiles target obsolete CUDA architectures such as compute_20 or sm_13.
- Some Rodinia builds require libcuda or external data absent from the local tree.
