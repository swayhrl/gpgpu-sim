# GPGPU-Sim Development Notes

_Last updated: 2026-05-01 — CCWS + DAWS 两篇论文复现完成；tools/paper_repro scaffold 验证通过；L3-lite supervisor scaffold 建立（AUTO-SUPERVISOR-0）；下一阶段进入 hrl/idea/cache-policy-experiments-v0 自研 cache policy。_

---

## 1. Project Overview

**目标**：在 GPGPU-Sim 上逐篇复现 GPU cache 相关论文，建立可复用的复现流程，并在此基础上开发自研 cache policy。

**主开发环境**：Linux Docker container（无真实 GPU）

| 项目 | 值 |
|------|-----|
| 当前活跃分支 | `hrl/paper/daws-repro-v0`（DAWS 已完成，下一步开新 branch） |
| CCWS 分支 | `hrl/paper/ccws-repro-v0`（已完成，不再修改） |
| 主 remote | `origin` → `git@github.com:swayhrl/gpgpu-sim.git` |
| 上游 remote | `upstream` → `https://github.com/gpgpu-sim/gpgpu-sim_distribution.git` |
| CUDA | 11.8 at `/usr/local/cuda-11.8` |
| GCC | 11.4.0 |

**关键目录**：

```
src/gpgpu-sim/          simulator 核心（shader.h / shader.cc / gpu-sim.cc）
configs/hrl-repro/      论文复现专用 config（不修改 tested-cfgs/）
experiments/paper-ccws/ CCWS 实验数据（CSV + round_state.yaml）
experiments/paper-daws/ DAWS 实验数据（CSV + round_state.yaml）
docs/papers/            论文阅读笔记 + repro plan + round 文档
tools/paper_repro/      论文复现自动化脚手架
/workspace/repos/gpgpu-workloads/  workload 管理框架（独立仓库）
```

---

## 2. Current Stable Checkpoint

**当前完成阶段**：CCWS + DAWS 两篇论文 approximate reproduction 完成

### CCWS Tags

| Tag | 说明 |
|-----|------|
| `baseline-a4ce3fe` | 上游 baseline 锚点 |
| `cache-inst-v0` | Cache instrumentation 稳定点 |
| `ccws-config-noop` | CCWS no-op config + stats |
| `ccws-would-gate-telemetry` | Would-gate telemetry |
| `ccws-load-gating-minimal` | 真实 load-only gating |
| `ccws-active-cutoff-resolution` | Active-warp cutoff 取舍决策 |
| `ccws-focused-validation` | Focused validation 完成 |
| `ccws-final-report` | **CCWS 复现最终报告** |
| `paper-repro-scaffold-v0` | tools/paper_repro scaffold |
| `paper-repro-scaffold-smoke` | scaffold AUTO-0 smoke test 通过 |
| `paper-repro-supervisor-v0` | L3-lite supervisor scaffold（AUTO-SUPERVISOR-0）|

### DAWS Tags（在 hrl/paper/daws-repro-v0 分支）

| Tag | 说明 |
|-----|------|
| `paper-repro-daws-config` | DAWS paper.yaml 创建 |
| `daws-reading-plan` | Reading stage + repro plan |
| `daws-config-noop` | No-op config + stats，feature_off 验证 |
| `daws-divergence-telemetry` | Divergence telemetry |
| `daws-would-throttle-telemetry` | Footprint + would-throttle telemetry |
| `daws-minimal-throttling` | Minimal real throttling + deadlock fix |
| `daws-focused-validation` | Focused validation 完成 |
| `daws-final-report` | **DAWS 复现最终报告** |

**feature_off quick regression 预期（两个 paper branch 均适用）**：
- sim_cycle = baseline（vecadd=5569）
- 所有 `paper_ccws_*` / `paper_daws_*` = 0

**当前稳定功能**：
- CCWS 完整机制链路：VTA-like probe → LLS score → would-gate telemetry → load-only gating
- DAWS 完整机制链路：divergence probe → footprint estimate → would-throttle → real throttle gate
- 两个 paper branch 的 feature_off 全程不破坏 baseline
- `tools/paper_repro/` 脚手架可用，AUTO-0 smoke test 通过

**不视为完成的 WIP**：
- CCWS faithful reproduction（miss-side VTA 近似、max_warps cutoff 近似）
- DAWS faithful reproduction（无 loop detection、无 load classification）
- 两个 paper branch 的 standard validation（cycle 方向相反，不建议跑）
- 自研 cache policy（尚未开 idea branch）

---

## 3. Recent Session Summary

**本次 save-session 覆盖的工作**：DAWS AUTO-3 → AUTO-8（完整 DAWS 复现试跑）

| Round | 内容 | src 改动 |
|-------|------|---------|
| AUTO-3 | DAWS no-op config + 7 knobs + 11 stats，feature_off 验证 | shader.h/cc, gpu-sim.cc |
| AUTO-4 | Divergence telemetry（passive，不改调度） | shader.h/cc |
| AUTO-5 | Footprint estimate + would-throttle telemetry | shader.h/cc, gpu-sim.cc |
| AUTO-6 | Minimal real throttling + deadlock fix（pre-scoreboard footprint + streak counter） | shader.h/cc |
| AUTO-7 | Focused validation：7 workloads × 3 configs | 无 |
| AUTO-8 | DAWS final reproduction report | 无 |

**修改的文件类型**：
- `src/gpgpu-sim/shader.h`、`shader.cc`、`gpu-sim.cc`（AUTO-3/4/5/6）
- `configs/hrl-repro/`（新增 5 个 DAWS config 目录）
- `docs/papers/`（新增 6 个 DAWS round 文档 + final report）
- `experiments/paper-daws/`（CSV + round_state.yaml）
- `tools/paper_repro/papers/daws.yaml`（更新）
- `CLAUDE.md`（更新）

**是否改 src/**：AUTO-3/4/5/6 改了；AUTO-7/8 未改

**已运行的测试（DAWS focused validation，AUTO-7）**：

| Workload | baseline | would_throttle_on | throttle_on | throttle_block |
|----------|----------|-------------------|-------------|----------------|
| vecadd | 5569 | 5569（cycle 不变） | 5569（0%） | 0 |
| rodinia_hotspot | 6931 | 6931（cycle 不变） | 7072（+2.0%） | 79030 |
| rodinia_srad_v2 | 8236 | 8236（cycle 不变） | 8561（+3.9%） | 38769 |
| rodinia_bfs | 6665 | 6665（cycle 不变） | 6665（0%） | 0 |
| rodinia_pathfinder | 6487 | 6487（cycle 不变） | 6487（0%） | 0 |
| polybench_2dconv | 6652 | 6652（cycle 不变） | 6652（0%） | 0 |
| polybench_fdtd2d | 5840 | 5840（cycle 不变） | 5840（0%） | 0 |

全部无 deadlock ✓，feature_off 不变 ✓

---

## 4. Known Issues / Limitations

### CCWS 近似实现限制

| 限制 | 影响 | 修复方向 |
|------|------|---------|
| VTA miss-side 近似（intra-warp repeated miss，非 eviction-based） | 信号方向正确但语义不同 | 在 `cache_block_t` 加 `warp_id` 字段 |
| cutoff 使用 `max_warps=64`（应为 active ~8） | cutoff 高估 8×；gate 触发时机偏晚；cycle 方向相反 | 需要更大 workload |
| 静态 `lls_hit_increment`（非动态 LLDS 公式） | inc=1 信号弱，inc=50 over-gating | 实现 VTAHitsTotal/InstIssuedTotal 比率计算 |
| `lg_score_threshold` 必须 ≥ `lls_base_score`（默认 100） | threshold < 100 → deadlock | 不要调低 threshold |

### DAWS 近似实现限制

| 限制 | 影响 | 修复方向 |
|------|------|---------|
| 无 loop detection | 非 loop 区域也触发 throttle | 实现 back-edge detection |
| 无 load classification | 所有指令均可被 gate（非 load-only） | 加 per-static-PC 分类表 |
| Streak counter（max=8）防死锁 | 非原论文机制，但功能等价 | 实现原论文 warp 选择保证 |
| tiny workload（64×64） | cycle 方向相反（增加而非减少） | 使用 256×256 或更大 workload |

### 通用规则

- CCWS：`lg_score_threshold >= lls_base_score`（否则 deadlock）
- DAWS：`gpgpu_daws_footprint_threshold` 默认 32（= warp_size），低于此值 gate 不触发
- standard validation 不建议（两个 paper branch 的 cycle 方向均相反）
- 自研 cache policy 不得进入 paper branch

---

## 5. Next Round Plan

**建议下一步**：开 `hrl/idea/cache-policy-experiments-v0` 做自研 cache policy

**不要做**：
- 继续修改 `hrl/paper/ccws-repro-v0` 或 `hrl/paper/daws-repro-v0` 的论文机制
- 在 paper branch 混入自研改进
- 跑 CCWS / DAWS standard validation（cycle 方向错误）

**如果要开第三篇论文**：
```bash
# 1. 复制 paper config 模板
cp tools/paper_repro/schemas/paper_config.example.yaml tools/paper_repro/papers/<key>.yaml
# 2. 填写 paper.yaml
# 3. 生成第一个 stage prompt
python3 tools/paper_repro/make_round_prompt.py --paper <key> --stage 00_reading
```

**如果要开自研 cache policy**：
```bash
git checkout main
git checkout -b hrl/idea/cache-policy-experiments-v0
# 可以复用 CCWS/DAWS 的 instrumentation 基础设施
```

**10 分钟 checkpoint 规则**：每轮 prompt 必须包含"如果执行超过 10 分钟，暂停并输出 checkpoint summary"。

---

## 6. Development Rules

### 每轮开始前必须执行

```bash
git branch --show-current   # 确认在正确分支
git status --short          # 确认工作树干净
```

### 通用规则

- **feature flag 默认 off**：所有行为改动必须 `gpgpu_enable_<key>=0` 时无变化
- **三组验证**：baseline / feature_off / feature_on；`feature_off ≈ baseline` 是第一成功标准
- **不使用 `git add .`**：只 stage 明确需要的文件
- **不提交**：build 产物、log、waveform、secret、`runs/latest_summary.csv`
- **不把 WIP test 加入 regression**
- **每轮结束必须更新 `round_state.yaml`**
- **自研实验不混入 paper branch**
- **单轮超过 10 分钟必须输出 checkpoint summary 并暂停**

### 论文复现工作流

```
tools/paper_repro/
  check_repo_clean.sh          # 前置检查
  make_round_prompt.py         # 生成 stage prompt
  stage_guard.sh               # 10 分钟 checkpoint 提示
  papers/ccws.yaml             # CCWS 配置
  papers/daws.yaml             # DAWS 配置
  templates/00_reading.md      # Stage 0: 论文阅读
  templates/01_noop.md         # Stage 1: no-op config
  templates/02_telemetry.md    # Stage 2: instrumentation
  templates/03_would_change.md # Stage 3: would-change telemetry
  templates/04_minimal_mechanism.md  # Stage 4: minimal mechanism
  templates/05_focused_validation.md # Stage 5: focused validation
  templates/06_standard_validation.md # Stage 6: standard validation（通常跳过）
  templates/07_final_report.md # Stage 7: final report
  schemas/                     # round_state / paper_config / result_csv 规范
```

---

## 7. Environment

```bash
# 必须在每个新 shell 中执行
source /workspace/repos/load_gpgpusim.sh
# 或手动：
export CUDA_INSTALL_PATH=/usr/local/cuda-11.8
export PATH=$CUDA_INSTALL_PATH/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_INSTALL_PATH/lib64:$CUDA_INSTALL_PATH/lib:$LD_LIBRARY_PATH
```

**Build**：

```bash
cd /workspace/repos/gpgpu-sim_distribution
source setup_environment release
make -j$(nproc)
# 输出：lib/gcc-11.4.0/cuda-11080/release/libcudart.so
# 状态：builds successfully（warnings only，Wreorder in gpu-cache.h — benign）
```

**Baseline**（vecadd，256-element float vector addition）：

| Metric | Value |
|--------|-------|
| `gpu_tot_sim_cycle` | 5569 |
| `gpu_tot_sim_insn` | 5376 |
| `gpu_tot_ipc` | 0.9653 |
| L2 miss rate | 0.3333 |

---

## 8. Workload Sets

**Quick set（7 workloads，smoke test）**：
vecadd, strided_access, page_stride_access, atomic_contention, mutual_tiled, polybench_2dconv, rodinia_hotspot

**DAWS focused set（7 workloads）**：
vecadd, rodinia_hotspot, rodinia_srad_v2, rodinia_bfs, rodinia_pathfinder, polybench_2dconv, polybench_fdtd2d

**CCWS focused set（7 workloads）**：
rodinia_hotspot, rodinia_srad_v2, polybench_fdtd2d, strided_access, page_stride_access, mutual_tiled, rodinia_bfs

**Standard set（13 workloads）**：所有 ready workload

**Cache policy focused set（7 workloads，WORKLOAD-AUDIT 确定）**：
rodinia_hotspot, rodinia_srad_v2, polybench_fdtd2d, mutual_tiled, polybench_2dconv, strided_access, parboil_histo

**Cache policy controls（5 workloads）**：
vecadd, polybench_gemm, mutual_naive, rodinia_backprop, atomic_contention

**运行命令**：

```bash
cd /workspace/repos/gpgpu-workloads
GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/<config_dir> \
  bash scripts/run_one.sh <workload_name>
```

---

## 9. CCWS Implementation Summary

**论文**：Cache-Conscious Wavefront Scheduling (Rogers, O'Connor, Aamodt — MICRO 2012)
**分支**：`hrl/paper/ccws-repro-v0`（已完成，不再修改）
**实现状态**：Approximate reproduction（机制链路正确，定量结果与原论文不符）

| 机制 | 实现状态 | 近似说明 |
|------|---------|---------|
| VTA probe | ✓ miss-side 近似 | 非 eviction-based |
| LLS score + decay | ✓ 静态 increment | 非动态 LLDS 公式 |
| Would-gate telemetry | ✓ | cutoff 使用 max_warps |
| Load-only gating | ✓ | 同上 |
| feature_off 不破坏 baseline | ✓ 全程验证 | — |

**Config knobs**（21 个，全部 default 0/off）：
`gpgpu_enable_ccws`, `gpgpu_ccws_enable_vta_probe`, `gpgpu_ccws_enable_lls_score`,
`gpgpu_ccws_lls_base_score(100)`, `gpgpu_ccws_lls_hit_increment(1)`,
`gpgpu_ccws_lls_decay_interval(100)`, `gpgpu_ccws_enable_would_gate`,
`gpgpu_ccws_enable_load_gating`, `gpgpu_ccws_lg_score_threshold(100)` 等

**关键约束**：`lg_score_threshold >= lls_base_score`（否则 deadlock）

---

## 10. DAWS Implementation Summary

**论文**：Divergence-Aware Warp Scheduling (Rogers, O'Connor, Aamodt — MICRO 2013)
**分支**：`hrl/paper/daws-repro-v0`（已完成，不再修改）
**实现状态**：Approximate reproduction（机制链路正确，cycle 方向受 tiny workload 限制）

| 机制 | 实现状态 | 近似说明 |
|------|---------|---------|
| Divergence probe | ✓ `active_count < warp_size` | 直接读 active_mask |
| Footprint estimate | ✓ `warp_size - active_count` | 无 loop detection |
| Would-throttle telemetry | ✓ | threshold=32 |
| Real throttle gate | ✓ | streak counter 防死锁（max=8） |
| feature_off 不破坏 baseline | ✓ 全程验证 | — |

**Config knobs**（7 个，全部 default 0/off）：
`gpgpu_enable_daws`, `gpgpu_daws_enable_telemetry`, `gpgpu_daws_enable_would_throttle`,
`gpgpu_daws_enable_throttling`, `gpgpu_daws_footprint_threshold(32)`,
`gpgpu_daws_min_active_threads(1)`, `gpgpu_daws_debug`

**Deadlock fix**：footprint 在 pre-scoreboard 用只读 `get_active_mask()` 更新（`daws_update_footprint_pre()`），避免被 gate 的 warp 永远无法刷新 footprint。

**Focused validation 结果（throttle_on，threshold=32）**：

| Workload | baseline | throttle_on | cycle_delta | throttle_block |
|----------|----------|-------------|-------------|----------------|
| vecadd | 5569 | 5569 | 0% | 0 |
| rodinia_hotspot | 6931 | 7072 | +2.0% | 79030 |
| rodinia_srad_v2 | 8236 | 8561 | +3.9% | 38769 |
| rodinia_bfs | 6665 | 6665 | 0% | 0 |
| rodinia_pathfinder | 6487 | 6487 | 0% | 0 |
| polybench_2dconv | 6652 | 6652 | 0% | 0 |
| polybench_fdtd2d | 5840 | 5840 | 0% | 0 |

cycle 方向相反（应减少）原因：tiny workload，throttle 阻止有效 warp 发射而非减少 cache pressure。

---

## 11. Next Steps

- [x] Round S–AK：CCWS paper reproduction 完成（tag `ccws-final-report`）
- [x] FINAL-INFRA：`tools/paper_repro/` scaffold 建立（tag `paper-repro-scaffold-v0`）
- [x] AUTO-0：scaffold smoke test 通过（tag `paper-repro-scaffold-smoke`）
- [x] AUTO-1：DAWS paper.yaml 创建（tag `paper-repro-daws-config`）
- [x] AUTO-2：DAWS reading stage 完成（tag `daws-reading-plan`）
- [x] AUTO-3：DAWS no-op config + stats，feature_off 验证通过（tag `daws-config-noop`）
- [x] AUTO-4：DAWS divergence telemetry，hotspot/bfs 有信号，sim_cycle 不变（tag `daws-divergence-telemetry`）
- [x] AUTO-5：DAWS footprint + would-throttle telemetry，hotspot/srad_v2 有信号（tag `daws-would-throttle-telemetry`）
- [x] AUTO-6：DAWS minimal real throttling，deadlock fix，throttle_block 有信号（tag `daws-minimal-throttling`）
- [x] AUTO-7：DAWS focused validation，7 workloads × 3 configs，无 deadlock（tag `daws-focused-validation`）
- [x] AUTO-8：DAWS final reproduction report（tag `daws-final-report`）
- [x] WORKLOAD-AUDIT：cache policy workload coverage audit 完成（23 workloads 审计，7 focused + 5 controls 确定）
- [x] AUTO-SUPERVISOR-0：L3-lite unattended supervisor scaffold 建立（supervisor.py + run_queue.sh + stop_rules.md + README_unattended.md）
- [x] AUTO-SUPERVISOR-1：supervisor smoke test 通过（3 jobs，stop rules 正确，runs/ gitignore 验证）
- [x] AUTO-PREPLAN-0：tomorrow queue + GPT review preplan templates（risk_policy, paper_preplan_template, job_queue.tomorrow.template, gpt_review_stub）
- [x] AUTO-SUPERVISOR-2：Codex CLI reviewer stub smoke test（codex v0.125.0 可用；stub dry-run 通过；high-risk stage 强制 blocked）
- [x] TOMORROW-RUN-PREP：tomorrow queue end-to-end rehearsal 通过（4 jobs，low/medium/high risk 判断全部正确）
- [x] TOMORROW-PAPERS：PCAL + Mascar preplan / paper.yaml / reading prompt / job_queue.tomorrow.yaml 准备完成（5 jobs dry-run 通过）
- [x] PCAL-Phase0：preflight 通过，reading 产物确认
- [x] PCAL-Phase1：no-op config + stats 完成（7 knobs + 9 stats；feature_off/noop 均不改 cycle；tag 待定）
- [x] PCAL-Phase2：cache pressure telemetry 完成（window_size=8；4 workloads 信号合理；sim_cycle 不变）
- [ ] **下一步**：PCAL Phase 3 would-change bypass telemetry

---

## 12. Known Limitations（环境）

| Issue | Detail |
|-------|--------|
| No real GPU | `nvidia-smi` unavailable in container |
| PTX simulation only | No real SASS execution |
| Slow simulation | ~203268× slower than real hardware — keep kernels small |
| Env not persistent | Re-source `load_gpgpusim.sh` in each new shell |
| No cuDNN/cuBLAS | Not installed |
