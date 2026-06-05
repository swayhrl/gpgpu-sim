# Mascar W21 Guidance: Rodinia/Parboil Full-Suite Blocker Fix

## 总体目标

- 修复 W19/W20 全套 Rodinia / Parboil workload blocker。
- 优先保证 build / binary / command / wrapper / data 可用性。
- 提升 ready workload 数量，支持 W22/W23 smoke 和测量。
- 每轮小任务可大胆修复，但 Mascar M1–M4 机制不可修改。
- 夜间无人值守执行，允许 Codex 多小轮尝试。

---

## W21A: Parboil build driver & python compatibility

### 目标

- 修复 Parboil build driver 环境：
  - Python 版本问题（python3 vs python2）
  - 构建脚本路径和权限
- 解决 Parboil raw source build 阻塞：
  - lbm, mrig-1/2/3, sad-1/2, cutcp, tpacf

### 任务

1. 检查 build.sh / Makefile 指定的 Python。
2. 替换 python -> python3，保持兼容性。
3. 确认数据目录路径。
4. dry-run 构建每个 blocker workload。
5. 更新 build_results.csv，记录每个尝试状态。
6. 日志存档到 /workspace/tmp。

### 输出

- experiments/suites/common/build_results_w21a.csv
- docs/papers/mascar_w21a_parboil_build_fix_report.md
- experiments/suites/audit/W21/w21a_postcheck.md

---

## W21B: Legacy CUDA arch / unsupported compiler

### 目标

- 修复 Rodinia legacy CUDA arch 阻塞：
  - compute_20, sm_13, sm_30 等旧 arch
- 处理 compiler incompatibility / nvcc flags
- 阻塞 workload：
  - leuko-1/2/3, mummer, particle, lavaMD

### 任务

1. grep Makefile / CMakeLists.txt / build.sh 中 CUDA arch。
2. 替换为当前支持 arch，如 sm_70 / sm_75 / sm_80。
3. 调整 nvcc/g++ flags 保证构建成功。
4. 测试 dry-run。
5. 记录每个 workload build_attempt 和 exit_code。
6. 更新 blocker manifest / notes。

### 输出

- experiments/suites/common/build_results_w21b.csv
- docs/papers/mascar_w21b_cuda_arch_fix_report.md
- experiments/suites/audit/W21/w21b_postcheck.md

---

## W21C: libcuda / missing dependency / environment fix

### 目标

- 修复 Rodinia / Parboil workload 链接或动态依赖：
  - libcuda, system dynamic libs, toolchain mismatch
- 阻塞 workload：
  - leuko-1/2/3, mummer, particle, lavaMD

### 任务

1. grep build logs 查找链接错误。
2. 补充环境变量或 wrapper override。
3. 局部替换 binary search path。
4. dry-run验证是否可构建。
5. 标注 blocker / next_action。
6. 日志存档 / postcheck。

### 输出

- experiments/suites/common/build_results_w21c.csv
- docs/papers/mascar_w21c_dependency_fix_report.md
- experiments/suites/audit/W21/w21c_postcheck.md

---

## W21D: Missing data / command normalization

### 目标

- 修复 binary 有但缺 input data 或 command 参数。
- 确保 wrapper 可 dry-run / smoke。
- 标记正确 availability_status 和 wrapper_status。

### 任务

1. 找到 binary 对应的 input data。
2. 更新 wrapper/run_command，支持 --help / --dry-run / --print-command。
3. 测试 dry-run。
4. 更新 smoke-ready manifest。
5. 日志存档。
6. 生成 audit CSV 和报告。

### 输出

- experiments/suites/common/data_availability_results_w21d.csv
- experiments/suites/common/wrapper_command_normalization_results.csv
- docs/papers/mascar_w21d_data_command_fix_report.md
- experiments/suites/audit/W21/w21d_postcheck.md

---

## 核心要求

- 每个 blocker workload 至少尝试 2 小轮策略。
- Codex 可大胆操作，但不要破坏 M1–M4 机制。
- 每轮 dry-run / smoke / build 都必须 timeout，默认 1800s，可调大。
- raw logs 大文件打包到 /workspace/tmp，不提交 Git。
- postcheck必须包含 start_ts / end_ts / elapsed_sec / branch / HEAD。
- 记录每轮成功/失败、原因、下一步建议。

---

## 验证

- build_results / wrapper / data / smoke 结果存在。
- blocker manifest 有更新。
- dry-run / smoke 能成功执行至少一个 workload。
- CSV headers正确，ready rows更新。
- postcheck生成。
- review pack完整。

---

## Stop conditions

- workspace 不干净
- Python/bash 不可用
- repository被破坏
- 全局 runner/collector不可用

Do not stop因为某个 workload仍不可 build/run。记录 blockers 和 next_action。
