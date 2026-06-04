# Mascar W9–W14 Guidance: Full Table III Workload Coverage Nightly Execution

## 总体目标
- 从 W7 activation-ready subset 开始，完成 M3 激活、Table III workload 扩展、memory/compute sweep、机制激活趋势分析和通用 framework 收口。
- 夜间无人值守，允许长时间运行。
- 每轮至少尝试 2 小轮，W9 最多 5 小轮尝试 M3 激活。

---

## W9: M3 Activation Input Search
### 目标
- 激活 M3 counters (non-owner hit-only)
- 小轮尝试不同 workload input/command variations
- 记录 notes/debug
- 输出：
  - W9 activation-ready subset manifest
  - W9 counter summary
  - W9 postcheck
  - W9 report

### 小轮策略
- 每个 workload最少2小轮尝试，最多5小轮
- 若小轮失败，立即 debug / 修 wrapper / config
- 必须保留 placeholder / unavailable rows

---

## W10: Table III Workload Expansion
### 目标
- 将 placeholder / phase_unknown / missing binary 转成 ready workloads
- 尝试构建 binaries，生成 wrapper
- 更新 manifest
- 输出：
  - w10_workload_manifest_ready.csv
  - w10_postcheck.md
  - w10 report

---

## W11: Memory-Intensive Focused Sweep
### 目标
- baseline + M2 + M3 + M4 配置执行 memory-intensive workloads
- 收集 M2/M3/M4 counters
- 输出：
  - w11_results.csv
  - w11_summary.md
  - w11_postcheck.md
  - report

---

## W12: Compute-Intensive Safety Sweep
### 目标
- 验证 compute-intensive workloads 不退化
- 收集 IPC / cycles / M2/M3/M4 counters
- 输出：
  - w12_results.csv
  - w12_summary.md
  - w12_postcheck.md
  - report

---

## W13: Table III Coverage Report
### 目标
- 合并 W11/W12 数据
- 生成 30 workloads coverage matrix
- 显示 memory vs compute trend、M2/M3/M4 activation counts
- 输出：
  - full_coverage_report.md
  - table_iii_full_results.csv
  - table_iii_trend_summary.md
  - table_iii_coverage_manifest.csv

---

## W14: General Paper Workload Framework Closeout
### 目标
- 收口 runner / collector / manifest / scripts
- 支持后续论文复现
- 输出：
  - mascar_w14_general_framework.md
  - experiments/common/README.md
  - tools/paper_repro/README.md

---

## 多轮尝试策略
- W9: 每个 workload最多5小轮尝试，至少2小轮。
- W10–W14: 每轮至少2小轮迭代，遇 wrapper / runner / runtime bug立即 debug。
- 记录 start_ts / end_ts / elapsed_sec。
- raw logs 打包到 /workspace/tmp，不提交 Git。
