# Mascar W16 Guidance: Current-Simulator Energy Trend (Energy Reproduction A)

## Stage Position

- W16 大轮次目标：在 current-simulator（GPGPU-Sim 4.x + AccelWattch）环境下，对 Table III workload 子集运行能耗评估，完成能耗流程跑通，为后续论文/实验提供基础。
- 上游依赖：W15 M3 diagnostic完成，W7/W8/W9–W14已生成 ready workload 或 activation-ready subset。
- 输出目标：baseline vs M4 active 能耗趋势，不追论文绝对值。

## 总体策略

- 分为 W16A/W16B/W16C 多小轮次：
  - **W16A**: 能耗工具检查与配置补齐
  - **W16B**: 能耗 dry-run / 小规模 smoke
  - **W16C**: 能耗数据收集与初步报告生成
- 可夜间无人值守，允许每小轮长时间运行，建议设置大 timeout。
- Codex 可以大胆操作工具和配置，但必须确保 M1–M4机制不受影响。

---

## W16A: Energy Tool Audit and Configuration

### 目标
1. 检查 simulator 当前是否可用能耗模拟：
   - AccelWattch 模型或 power counters
   - 可提取 total energy, leakage, DRAM, L1/L2/cache, interconnect
2. 补充或生成必要的 energy/power config：
   - 建议单独 config 目录 `configs/hrl-repro/SM7_QV100_mascar_energy_on/`
   - 确保 baseline_off 和 M4 active 都能使用
3. 配置 collector 能正确解析能耗字段
4. 输出:
   - docs/papers/mascar_w16a_energy_tool_audit.md
   - experiments/paper-mascar/energy/e1_energy_stat_map.csv

### 验证
- 能耗字段可提取
- Collector 能输出 CSV
- 配置能够在 dry-run 下被 simulator 接受

---

## W16B: Energy Smoke / Dry-Run Execution

### 目标
1. 选取 W7/W8 6 个 ready workload 或 subset
2. 配置运行：
   - baseline_off
   - M4_reexec_load active
3. 执行 dry-run / 小规模 smoke：
   - 确认能耗字段生成
   - 确认不会 crash
4. 如果 wrapper/config/runtime报错，立即 debug 修复

### 输出
- experiments/paper-mascar/energy/w16b_energy_results.csv
- experiments/paper-mascar/energy/w16b_energy_summary.md
- experiments/paper-mascar/energy/w16b_postcheck.md

### 验证
- 每个 workload 生成至少总能耗/DRAM/L1/L2能耗
- cycles / IPC 与 baseline一致性检查
- timeout/crash/invalid log 记录

---

## W16C: Energy Trend Report

### 目标
1. 基于 W16B 收集的结果，生成能耗趋势报告
2. 输出：
   - docs/papers/mascar_w16c_energy_trend_report.md
   - experiments/paper-mascar/energy/w16c_energy_trend_summary.csv
3. 报告需明确：
   - current-simulator 能耗趋势
   - baseline vs M4 active 对比
   - 不声称复现论文 12% energy saving
   - 可供后续论文/实验复用

### 验证
- CSV、MD 生成
- baseline/M4 对比趋势可解释
- 所有字段有数据或明确标注 unavailable

---

## 小轮次策略

- W16A/B/C 每轮可迭代多次修复工具、config、wrapper
- Codex 可以大胆生成/修改 collector / config / wrapper
- 每轮必须记录 start_ts / end_ts / elapsed_sec
- 日志大文件打包到 `/workspace/tmp/`，不提交 Git

