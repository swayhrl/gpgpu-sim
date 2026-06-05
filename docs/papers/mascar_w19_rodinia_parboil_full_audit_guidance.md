# Mascar W19 Guidance: Rodinia / Parboil Full-Suite Availability Audit

## 总体目标

- 审计本地 Rodinia / Parboil 所有 benchmark，建立统一 manifest，保证可构建、可运行、可 dry-run / smoke。
- 为后续 Mascar / cache/MMU / L2 / CPU-GPU 协同实验提供全套 workload 复现基础。
- 本轮分 W19A/B/C/D 小轮次：
  - W19A：workload inventory audit
  - W19B：binary/source/data build/recovery
  - W19C：wrapper/command normalization
  - W19D：smoke validation + postcheck

## 背景

- W17–W18 已完成 Table III 30 row 的扩展和 kernel phase mapping。
- W16 完成能耗流程。
- W19 不局限论文 Table III，而是要覆盖 **Rodinia / Parboil 官方全部测试集**。
- 本轮核心是构建可运行闭环 + 全套 manifest + runner/collector。

---

## W19A: Workload Inventory Audit

### 目标
1. 发现本地 Rodinia / Parboil 所有可用 benchmark。
2. 分类：memory-intensive / compute-intensive / unknown。
3. 标记现状：
   - 已有 source
   - 已有 binary
   - 已有 input/data
   - 已有 wrapper/command
4. 输出 CSV/MD，形成全量 inventory。

### 输出
- experiments/suites/rodinia/rodinia_full_manifest.csv
- experiments/suites/parboil/parboil_full_manifest.csv
- docs/papers/mascar_w19a_full_suite_inventory_report.md
- experiments/suites/common/w19a_postcheck.md

---

## W19B: Binary/Source/Data Build & Recovery

### 目标
- 尝试构建缺 binary 的 workloads
- 尝试恢复缺 data / input
- 修复构建或工具问题
- 优先 memory-intensive workloads
- 每个 workload 尝试至少 2 build strategy

### 输出
- experiments/suites/rodinia/build_results.csv
- experiments/suites/parboil/build_results.csv
- docs/papers/mascar_w19b_build_recovery_report.md
- experiments/suites/common/w19b_postcheck.md

---

## W19C: Wrapper and Command Normalization

### 目标
- 生成统一 wrapper，支持 dry-run / print-command / --help
- 更新 command manifest
- 保留 placeholder / missing / unresolved workloads
- 支持 runner matrix
- 为 W19D smoke / dry-run做准备

### 输出
- experiments/suites/common/run_suite_wrappers.sh
- experiments/suites/common/suite_command_manifest.csv
- docs/papers/mascar_w19c_wrapper_command_normalization_report.md
- experiments/suites/common/w19c_postcheck.md
- wrappers/*.sh

---

## W19D: Smoke Validation & Postcheck

### 目标
- 对所有 ready wrappers 执行 dry-run 或小规模 smoke
- 检查 build成功、wrapper正确、命令能执行
- 收集 run_manifest, results.csv, summary.md, status_matrix.csv
- 生成最终 W19 closeout review pack

### 输出
- experiments/suites/results/w19_smoke_results.csv
- experiments/suites/results/w19_smoke_summary.md
- experiments/suites/results/w19_smoke_run_manifest.csv
- experiments/suites/results/w19_smoke_status_matrix.csv
- experiments/suites/audit/w19_postcheck.md
- docs/papers/mascar_w19d_smoke_validation_report.md
- /workspace/tmp/mascar_w19_full_suite_review_pack_YYYYMMDD_HHMMSS.tar.gz

---

## 核心规范

- 所有 workload / wrapper / manifest 都保留状态，标注 ready / placeholder / phase_pending / missing_binary / missing_data
- Dry-run smoke必须覆盖全部 ready workload
- 实际 smoke可选执行 subset（如 memory-intensive），必须 timeout
- 所有 output/CSV/MD 保留 reviewer 可读性
- Raw logs可打包到 /workspace/tmp，不提交 Git
- 不修改 Mascar M1-M4机制
- 不用 git add . 或 git add -A
- postcheck必须包含 start_ts/end_ts/elapsed_sec/branch/HEAD

---

## 验证

- 检查 CSV headers、row counts
- 检查 wrapper脚本语法 bash -n
- 检查 dry-run smoke是否覆盖全部 ready workload
- 检查 postcheck存在
- 检查 review pack打包完整

## Stop conditions

- workspace不干净
- 构建工具不可用
- Python解析器不可用
- 仓库被破坏

Do not stop因为部分 workload missing。文档中必须记录 blocker 和 next action。
