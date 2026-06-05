# INFRA Validation

Base infra validation checklist:

- `git diff --check`
- `bash -n experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh`
- `python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py`
- `python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py`
- suite runner/collector syntax checks when present
- no simulator source changes
- no Mascar configs
- no `experiments/paper-mascar` content

Trace infra validation additionally requires build validation and source diff review to ensure only default-off paperrepro kernel trace changes are present.
