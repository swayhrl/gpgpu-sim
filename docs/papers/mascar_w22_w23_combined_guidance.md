# Mascar W22-W23 Guidance: Expanded Smoke Promotion + Baseline Characterization

## 总体目标

- 在 W21 blocker fix 基础上，把 binary-recovered workload 推进 smoke-ready。
- 扩展 dry-run / smoke 到尽可能多的 ready workload。
- 对 smoke-ready workload 做 baseline characterization，收集 cycles/IPC/L1/L2/Mascar counters/energy stats。
- 提供通用基础设施数据，为 W24–W26 full sweep / energy / framework release 打基础。

---

## W22: Expanded Smoke Promotion

### 目标

1. 使用 W21 新增 binary-recovered workload candidate。
2. 补全 command / data / wrapper。
3. 运行 dry-run 和 bounded actual smoke。
4. 更新 manifest，标注 smoke-ready。
5. 日志存档 / postcheck。

### 小轮次

- 每个 workload 尝试 2 小轮策略：
  - command/data normalization
  - wrapper微修
  - 小规模 smoke
- 如果 smoke timeout 或 crash，记录 blocker，并继续其他 workload。

### 输出

- experiments/suites/results/W22/w22_smoke_results.csv
- experiments/suites/results/W22/w22_smoke_summary.md
- experiments/suites/results/W22/w22_status_matrix.csv
- experiments/suites/results/W22/w22_run_manifest.csv
- experiments/suites/audit/W22/w22_postcheck.md
- docs/papers/mascar_w22_smoke_validation_report.md
- 更新后的 manifest: experiments/suites/common/full_suite_ready_manifest.csv

### 验证

- 所有 ready workload 能 dry-run。
- smoke-ready workload 实际 smoke 成功。
- postcheck记录 start/end/elapsed/branch/HEAD/log path。
- raw logs打包到 /workspace/tmp，不提交 Git。

---

## W23: Baseline Characterization

### 目标

- 对 W22 smoke-ready workload，收集 baseline 性能和 Mascar counters。
- 指标：
  - cycles / IPC
  - L1/L2 cache hits/misses
  - Mascar M1–M4 counters
  - energy/power字段（如果 W16 可用）
  - timeout / crash / wrapper_status / smoke_status
- 生成初步统计报告，为后续 W24–W26 full sweep 做准备。

### 输出

- experiments/suites/results/W23/w23_baseline_results.csv
- experiments/suites/results/W23/w23_baseline_summary.md
- experiments/suites/results/W23/w23_baseline_status_matrix.csv
- experiments/suites/results/W23/w23_baseline_run_manifest.csv
- experiments/suites/audit/W23/w23_postcheck.md
- docs/papers/mascar_w23_baseline_characterization_report.md

### 验证

- cycles/IPC/Mascar counters 能解析。
- 所有 smoke-ready workload 都至少 dry-run一次。
- CSV headers、manifest row counts 正确。
- Postcheck存在并记录日志路径、blocker、ready counts。

---

## 核心要求

1. 不修改 M1–M4 Mascar机制。
2. 不新建分支 / 不 fetch upstream。
3. Dry-run / smoke 对 ready workload。
4. placeholder / unavailable row 保留。
5. 每轮 dry-run/smoke都必须 timeout，默认 1800s，可适当加长。
6. raw logs大文件打包到 /workspace/tmp，不提交Git。
7. 每个 workload至少尝试2小轮。
8. 不用 git add . 或 git add -A。
9. postcheck必须 start_ts/end_ts/elapsed_sec。

---

## 小轮次策略

- W22A：command/data normalization
- W22B：wrapper更新 + smoke promotion
- W23A：baseline characterization dry-run
- W23B：baseline characterization actual run + stats收集

每小轮都要记录 notes/debug。

---

## Stop conditions

- workspace 不干净
- runner/collector/wrapper 脚本不可用
- Python/bash不可用
- 发生全局 crash 或 timeout
- 单个 workload failure 不停止，只记录 blocker和next_action

