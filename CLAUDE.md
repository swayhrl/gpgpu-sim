# GPGPU-Sim Development Notes

_Last updated: 2026-05-01 — DAWS paper reproduction 完成到 daws-final-report tag；CCWS + DAWS 两篇论文复现完成；后续进入 hrl/idea/cache-policy-experiments-v0 自研 cache policy。_

---

## 1. Project Overview

**目标**：在 GPGPU-Sim 上逐篇复现 GPU cache 相关论文，建立可复用的复现流程，并在此基础上开发自研 cache policy。

**主开发环境**：Linux Docker container（无真实 GPU）

| 项目 | 值 |
|------|-----|
| 当前分支 | `hrl/paper/ccws-repro-v0` |
| 主 remote | `origin` → `git@github.com:swayhrl/gpgpu-sim.git` |
| 上游 remote | `upstream` → `https://github.com/gpgpu-sim/gpgpu-sim_distribution.git` |
| CUDA | 11.8 at `/usr/local/cuda-11.8` |
| GCC | 11.4.0 |

**关键目录**：

```
src/gpgpu-sim/          simulator 核心（shader.h / shader.cc / gpu-sim.cc）
configs/hrl-repro/      论文复现专用 config（不修改 tested-cfgs/）
experiments/paper-ccws/ CCWS 实验数据（CSV + round_state.yaml）
docs/papers/            论文阅读笔记 + repro plan + round 文档
tools/paper_repro/      论文复现自动化脚手架（FINAL-INFRA 新增）
/workspace/repos/gpgpu-workloads/  workload 管理框架（独立仓库）
```

---

## 2. Current Stable Checkpoint

**当前完成阶段**：CCWS paper reproduction Round AI + AK + FINAL-INFRA

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

**当前 feature_off quick regression 预期**：7/7 pass，sim_cycle = baseline，所有 `paper_ccws_*` = 0。

**当前稳定功能**：
- CCWS 完整机制链路：VTA-like probe → LLS score → would-gate telemetry → load-only gating
- feature_off 全程不破坏 baseline（所有 round 验证通过）
- `tools/paper_repro/` 脚手架可用

**不视为完成的 WIP**：
- CCWS faithful reproduction（miss-side VTA 近似、max_warps cutoff 近似）
- standard validation（cycle 方向相反，不建议跑）
- 自研 cache policy（尚未开 idea branch）

---

## 3. Recent Session Summary（本次 save-session 覆盖的工作）

**完成的 Rounds**：Round AI → Round AK → FINAL-INFRA

| Round | 内容 | src 改动 |
|-------|------|---------|
| AI | Focused validation：7 workload × conservative(inc=1,th=100) / aggressive(inc=50,th=100) | 无 |
| AK | CCWS final reproduction report 撰写 | 无 |
| FINAL-INFRA | tools/paper_repro/ scaffold 建立 | 无 |

**修改的文件类型**：
- docs/papers/（新增 ccws_round_ai_focused_validation.md、ccws_final_reproduction_report.md）
- experiments/paper-ccws/（新增 focused_ccws_validation.csv、final_summary.csv）
- tools/paper_repro/（新增 16 个文件）
- CLAUDE.md、ccws_repro_plan.md、round_state.yaml（更新）

**是否改 src/**：否（Round AI 起不再改 src/）

**已运行的测试**：
- Round AI focused validation：7 workload × 3 configs = 21 runs，全部 gpgpusim_exit=1
- conservative(inc=1,th=100)：hotspot/srad_v2/fdtd2d/mutual_tiled/bfs 有 lg_block>0，cycle +2–11%
- aggressive(inc=50,th=100)：严重 over-gating，cycle +390–1464%
- th=99：全部 deadlock（threshold < lls_base_score=100）

---

## 4. Known Issues / Limitations

### CCWS 近似实现限制

| 限制 | 影响 | 修复方向 |
|------|------|---------|
| VTA miss-side 近似（intra-warp repeated miss，非 eviction-based） | 信号方向正确但语义不同 | 在 `cache_block_t` 加 `warp_id` 字段 |
| cutoff 使用 `max_warps=64`（应为 active ~8） | cutoff 高估 8×；gate 触发时机偏晚；cycle 方向相反 | 需要更大 workload 才能用 active-warp cutoff |
| 静态 `lls_hit_increment`（非动态 LLDS 公式） | inc=1 信号弱，inc=50 over-gating，无稳定中间点 | 实现 VTAHitsTotal/InstIssuedTotal 比率计算 |
| tiny workload（64×64，2–8 warps/SM） | 不代表论文 HCS 场景 | 使用 256×256 或更大 workload |

### 重要规则

- `lg_score_threshold` 必须 ≥ `lls_base_score`（默认 100），否则 deadlock
- standard validation 不建议（cycle 方向相反，会产生误导性结果）
- 自研 cache policy 不得进入 `hrl/paper/ccws-repro-v0`

---

## 5. Next Round Plan

**建议下一步**：开 `hrl/idea/cache-policy-experiments-v0` 做自研 cache policy

**不要做**：
- 继续修改 `hrl/paper/ccws-repro-v0` 的 CCWS 机制
- 在 paper branch 混入自研改进
- 跑 CCWS standard validation（cycle 方向错误）

**如果要开第二篇论文**：
```bash
# 1. 复制 paper config 模板
cp tools/paper_repro/schemas/paper_config.example.yaml tools/paper_repro/papers/<key>.yaml
# 2. 填写 paper.yaml
# 3. 生成第一个 stage prompt
python3 tools/paper_repro/make_round_prompt.py --paper <key> --stage 00_reading
```

**10 分钟 checkpoint 规则**：每轮 prompt 必须包含"如果执行超过 10 分钟，暂停并输出 checkpoint summary"。

---

## 6. Development Rules

### 每轮开始前必须执行

```bash
git branch --show-current   # 确认在正确分支
git status --short          # 确认工作树干净
# 或使用脚手架：
bash tools/paper_repro/check_repo_clean.sh hrl/paper/ccws-repro-v0 ccws-final-report
```

### 通用规则

- **feature flag 默认 off**：所有行为改动必须 `gpgpu_enable_<key>=0` 时无变化
- **三组验证**：baseline / feature_off / feature_on；`feature_off ≈ baseline` 是第一成功标准
- **不使用 `git add .`**：只 stage 明确需要的文件
- **不提交**：build 产物、log、waveform、secret、`runs/latest_summary.csv`
- **不把 WIP test 加入 regression**
- **每轮结束必须更新 `round_state.yaml`**
- **自研实验不混入 paper branch**

### 论文复现工作流

```
tools/paper_repro/
  check_repo_clean.sh          # 前置检查
  make_round_prompt.py         # 生成 stage prompt
  stage_guard.sh               # 10 分钟 checkpoint 提示
  papers/ccws.yaml             # CCWS 配置（样例）
  templates/00_reading.md      # Stage 0: 论文阅读
  templates/01_noop.md         # Stage 1: no-op config
  templates/02_telemetry.md    # Stage 2: instrumentation
  templates/03_would_change.md # Stage 3: would-change telemetry
  templates/04_minimal_mechanism.md  # Stage 4: minimal mechanism
  templates/05_focused_validation.md # Stage 5: focused validation
  templates/06_standard_validation.md # Stage 6: standard validation
  templates/07_final_report.md # Stage 7: final report
  schemas/                     # round_state / paper_config / result_csv 规范
```

---

## 7. Environment

```bash
# 必须在每个新 shell 中执行
export CUDA_INSTALL_PATH=/usr/local/cuda-11.8
export PATH=$CUDA_INSTALL_PATH/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_INSTALL_PATH/lib64:$CUDA_INSTALL_PATH/lib:$LD_LIBRARY_PATH
# 或使用一键脚本：
source /workspace/repos/load_gpgpusim.sh
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

**Focused set（7 workloads，mechanism validation）**：
rodinia_hotspot, rodinia_srad_v2, polybench_fdtd2d, strided_access, page_stride_access, mutual_tiled, rodinia_bfs

**Standard set（13 workloads）**：所有 ready workload（不含 pathfinder/lud 等低 L1D miss 率 workload）

**运行命令**：

```bash
cd /workspace/repos/gpgpu-workloads
GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/SM7_QV100_ccws_noop_off \
  bash scripts/run_one.sh rodinia_hotspot
```

---

## 9. CCWS Implementation Summary

**论文**：Cache-Conscious Wavefront Scheduling (Rogers, O'Connor, Aamodt — MICRO 2012)

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

**Focused validation 结果（conservative inc=1, th=100）**：

| Workload | baseline | conservative | cycle_delta | lg_block |
|----------|----------|-------------|-------------|----------|
| rodinia_hotspot | 6,931 | 7,343 | +6% | 4,595 |
| rodinia_srad_v2 | 15,926 | 16,739 | +5% | 17,092 |
| polybench_fdtd2d | 35,681 | 39,539 | +11% | 54,907 |
| strided_access | 5,825 | 5,825 | 0% | 0 |
| page_stride_access | 5,851 | 5,851 | 0% | 0 |
| mutual_tiled | 7,479 | 7,921 | +6% | 2,014 |
| rodinia_bfs | 136,110 | 139,060 | +2% | 4,341 |

cycle 方向相反（应减少）原因：cutoff 高估 8×，gate 触发时机偏晚。

---

## 10. Next Steps

- [x] Round S–AK：CCWS paper reproduction 完成（tag `ccws-final-report`）
- [x] FINAL-INFRA：`tools/paper_repro/` scaffold 建立（tag `paper-repro-scaffold-v0`）
- [x] AUTO-0：scaffold smoke test 通过（tag `paper-repro-scaffold-smoke`）
- [x] AUTO-1：DAWS paper.yaml 创建（tag `paper-repro-daws-config`）
- [x] AUTO-2：DAWS reading stage 完成（tag `daws-reading-plan`）
- [x] AUTO-3：DAWS no-op config + stats 添加，feature_off 验证通过（tag `daws-config-noop`）
- [x] AUTO-4：DAWS divergence telemetry 实现，hotspot/bfs 有信号，sim_cycle 不变（tag `daws-divergence-telemetry`）
- [x] AUTO-5：DAWS footprint + would-throttle telemetry，hotspot/srad_v2 有信号，sim_cycle 不变（tag `daws-would-throttle-telemetry`）
- [x] AUTO-6：DAWS minimal real throttling，streak-based deadlock prevention，hotspot/srad_v2 throttle_block 有信号（tag `daws-minimal-throttling`）
- [x] AUTO-7：DAWS focused validation，7 workloads × 3 configs，无 deadlock，feature_off 不变（tag `daws-focused-validation`）
- [x] AUTO-8：DAWS final reproduction report（tag 待提交 `daws-final-report`）
- [ ] **下一步**：进入 `hrl/idea/cache-policy-experiments-v0` 自研 cache policy

**DAWS 论文**：Rogers/O'Connor/Aamodt — MICRO 2013（与 CCWS 同一作者组）
**DAWS 核心**：divergence → footprint prediction → warp throttling（proactive，比 CCWS reactive 更激进）
**DAWS 近似方案**：用 `active_count()` 作为 footprint proxy，跳过 loop detection 和 load classification
**DAWS 最高风险**：loop detection 缺失可能导致 over-throttle；tiny workload 可能无 divergence 信号

---

## 11. Known Limitations（环境）

| Issue | Detail |
|-------|--------|
| No real GPU | `nvidia-smi` unavailable in container |
| PTX simulation only | No real SASS execution |
| Slow simulation | ~203268× slower than real hardware — keep kernels small |
| Env not persistent | Re-source `load_gpgpusim.sh` in each new shell |
| No cuDNN/cuBLAS | Not installed |
