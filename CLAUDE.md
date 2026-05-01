# GPGPU-Sim Development Notes

_Last updated: 2026-05-01 — Round AG complete; confirmed cutoff bug: uses max_warps(64) not active_warps(~8); inactive warp base_score consumes 87.5% of cutoff budget. **Round AG 变更尚未提交（working tree dirty）。**_

## Git 工作流

### Remote 配置

| remote | URL |
|--------|-----|
| `origin` | `git@github.com:swayhrl/gpgpu-sim.git`（个人 fork） |
| `upstream` | `https://github.com/gpgpu-sim/gpgpu-sim_distribution.git`（官方上游） |

### 已 push 分支 & tag

| 类型 | 名称 | 说明 |
|------|------|------|
| 分支 | `dev` | 跟踪上游，不做实验改动 |
| 分支 | `hrl/project-notes` | Day1–Day4 阅读笔记、CLAUDE.md、`.claude/commands/` |
| 分支 | `hrl/tlb-latency-v0` | **当前实验分支**，TLB latency model 实验代码 |
| tag | `baseline-a4ce3fe` | 对应上游 commit `a4ce3fe`，baseline 锚点 |

### 代码修改前必须执行

```bash
git branch --show-current   # 期望：hrl/tlb-latency-v0
git status --short          # 期望：空（工作树干净）
```

### 注意事项

- **不要**重新设置 remote、重新创建 baseline tag、重新生成 SSH key
- `save-session.md` command 文件在 `hrl/project-notes` 分支，未合并到 `hrl/tlb-latency-v0`。如需 `/save-session`：
  ```bash
  git checkout hrl/project-notes -- .claude/commands/save-session.md
  ```

## Environment

- OS: Linux (Docker container, no real GPU)
- CUDA: 11.8 at `/usr/local/cuda-11.8`
- GCC: 11.4.0
- Branch: `dev`

### Required env vars (must be set before build or run)

```bash
export CUDA_INSTALL_PATH=/usr/local/cuda-11.8
export PATH=$CUDA_INSTALL_PATH/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_INSTALL_PATH/lib64:$CUDA_INSTALL_PATH/lib:$LD_LIBRARY_PATH
```

## 输出语言

- 默认使用中文回答。
- 专有名词、代码符号、类名、函数名、文件名、配置项、命令行参数保持英文原文。
- 技术解释可以中英混排，例如：`mem_fetch` 表示一次 memory request packet。
- 不要把代码标识符翻译成中文。
- 总结、计划、步骤、原因分析、风险说明请使用中文。
- 如果用户没有特别要求，输出应简洁、结构化、适合直接复制到 Markdown 笔记中。

### Load GPGPU-Sim runtime (after building)

```bash
source /workspace/repos/gpgpu-sim_distribution/setup_environment release
```

One-liner script at `/workspace/repos/load_gpgpusim.sh` combines both steps.

## Build

```bash
cd /workspace/repos/gpgpu-sim_distribution
export CUDA_INSTALL_PATH=/usr/local/cuda-11.8
source setup_environment release
make -j$(nproc)
```

Build output: `lib/gcc-11.4.0/cuda-11080/release/libcudart.so`

Status: **builds successfully** with warnings only (Wreorder in gpu-cache.h — benign).

## Running a CUDA app under simulation

```bash
# 1. Copy GPU config files into the app directory
cp configs/tested-cfgs/SM7_QV100/gpgpusim.config .
cp configs/tested-cfgs/SM7_QV100/config_volta_islip.icnt .

# 2. Load environment
source /workspace/repos/load_gpgpusim.sh

# 3. Run
./myapp
```

### Verified working test

```bash
cd /workspace/repos/test_vecadd
source /workspace/repos/load_gpgpusim.sh
./vecadd
```

Expected output tail:
```
gpu_sim_cycle = 5569
vecAdd result: PASS
GPGPU-Sim: *** exit detected ***
```

## Available GPU configs

```
configs/tested-cfgs/
  SM2_GTX480       # Fermi
  SM3_KEPLER_TITAN # Kepler
  SM6_TITANX       # Pascal
  SM75_RTX2060     # Turing
  SM75_RTX2060_S   # Turing (variant)
  SM7_QV100        # Volta (recommended, best validated)
  SM7_GV100        # Volta
  SM7_TITANV       # Volta (variant)
  SM86_RTX3070     # Ampere
```

## Baseline

Established 2026-04-28. Full details in `baseline_run_notes.md`.

**Benchmark:** `vecadd` — 256-element float vector addition, 1 kernel, 1 CTA.

**Config files are pre-copied** into `/workspace/repos/test_vecadd/` — no copy step needed for re-runs.

**Re-run command:**

```bash
cd /workspace/repos/test_vecadd
source /workspace/repos/load_gpgpusim.sh
./vecadd 2>&1 | tee /workspace/repos/gpgpu-sim-baseline.log
```

**Key baseline numbers** (from `/workspace/repos/gpgpu-sim-run.log`):

| Metric | Value |
|--------|-------|
| GPGPU-Sim version | 4.2.0 `[a4ce3fe]` |
| `gpu_tot_sim_cycle` | 5569 |
| `gpu_tot_sim_insn` | 5376 |
| `gpu_tot_ipc` | 0.9653 |
| L1D miss rate (core 0) | 1.000 (cold cache, expected) |
| L2 miss rate | 0.3333 |
| Wall time | 1 second |
| Silicon slowdown | 203268× |

**Stat extraction one-liner:**

```bash
grep -E "gpu_tot_sim_cycle|gpu_tot_sim_insn|gpu_tot_ipc|gpgpu_silicon_slowdown|L2_total_cache_miss_rate|averagemflatency|gpgpu_simulation_time" \
  /workspace/repos/gpgpu-sim-baseline.log
```

## Code Reading Progress (Day1–Day4)

Four days of deep read-through of the PTX functional simulation → timing model pipeline.

| Day | Topic | Key Conclusion |
|-----|-------|----------------|
| Day1 | Main loop / warp scheduler / scoreboard | `scoreboard stall` 根因：global load issue 后目标寄存器 reserve，直到 `releaseRegister()` 在 `writeback()` 被调用。Notes: `day1_main_loop_scheduler_notes.md` |
| Day2 | Coalescing / L1D / mem_fetch 生命周期 | `warp_inst_t` → `mem_access_t` → `mem_fetch`；L1D miss 进 `m_miss_queue`，经 `baseline_cache::cycle()` 离开 shader core。Notes: `day2_coalescing_l1d_notes.md` |
| Day3 | L2 / Interconnect / DRAM / return path | `mem_fetch` 全程状态机：`IN_ICNT_TO_MEM` → L2 → DRAM → `IN_ICNT_TO_SHADER` → `writeback()` → `releaseRegister()`。Notes: `Day3_L2_Interconnect_DRAM_Return_Path_Reading_Notes.md` |
| Day4 | TLB/VM hook 确认 / MMU 插入点 | 见下方 TLB/VM 现状节。无输出文件（报告在聊天中输出）。 |

### TLB/VM 现状（Day4 确认）

**GPGPU-Sim 主线没有任何 TLB/MMU/page walk 实现**，只有以下预留枚举（全部未被实际代码调用）：

| 文件 | 符号 | 状态 |
|------|------|------|
| `src/gpgpu-sim/mem_fetch_status.tup:34-35` | `IN_L1TLB_MISS_QUEUE`, `IN_VM_MANAGER_QUEUE` | 纯占位，未被 `set_status()` 调用 |
| `src/gpgpu-sim/stats.h:42` | `tlb_request_status { TLB_HIT, TLB_READY, TLB_PENDING }` | 未被任何逻辑使用 |
| `src/gpgpu-sim/stats.h:49` | `TLB_STALL`（在 `mem_stage_stall_type` 枚举中） | 未被任何 `stall_cond` 赋值 |

**地址语义**：`new_addr_type = unsigned long long`，当前代码不区分 VA/PA。`addrdec_tlx()`（`mem_fetch.cc:61`）是 DRAM 拓扑解码（linear → chip/bank/row/col），不是 VA→PA 翻译。

### 最小 TLB Latency Model 插入点（已分析，未实现）

推荐在 `ldst_unit` 内部新增 TLB 延迟队列，仿照 `l1_latency_queue`（`shader.h:1475`）模式：

| 路径 | 文件 | 插入位置 |
|------|------|----------|
| bypass L1D 路径 | `shader.cc:2298–2302` | `mem_fetch` 创建后、`m_icnt->push(mf)` 前 |
| 经 L1D 路径 | `shader.cc:2073–2082` | `mem_fetch` 创建后、放入 `l1_latency_queue` 前 |

**所需新增**（约 150–200 行，三个文件）：
- `simple_tlb_t` 结构体（per-SM LRU TLB，page number 索引）
- `tlb_latency_queue`（`shader.h`，复用 `l1_latency_queue` 结构）
- `tlb_latency_queue_cycle()` 函数（`shader.cc`）
- Config 参数：`-gpgpu_tlb_entries`, `-gpgpu_tlb_page_size`, `-gpgpu_tlb_hit_latency`, `-gpgpu_tlb_miss_latency`
- 统计项：`gpgpu_tlb_accesses`, `gpgpu_tlb_hits`, `gpgpu_tlb_misses`
- 与 scoreboard / `m_pending_writes` 兼容：无需修改，TLB delay 只延长 RTT，不改变 reserve/release 逻辑

## Next Steps

- [x] Round C：3 个 Tier 0 micro 全部就绪（strided_access / page_stride_access / atomic_contention）
- [x] Round D：mutual_naive + mutual_tiled 就绪；所有 6 个 Tier 0 workload 完成
- [x] Round E：stats extractor 增强，summarize_runs.py 新增，runs/latest_summary.csv 生成
- [x] Round F：PolyBench-GPU 4 benchmarks（gemm/atax/2dconv/fdtd2d）带入，全部在 GPGPU-Sim 跑通
- [x] **Round G**：Rodinia 4 benchmarks（pathfinder/hotspot/srad_v2/lud）带入，全部在 GPGPU-Sim 跑通；14 workload 全部 ready
- [x] **Round G**：Rodinia 4 benchmarks（pathfinder/hotspot/srad_v2/lud）带入，全部在 GPGPU-Sim 跑通；14 workload 全部 ready
- [x] **Round O**：Cache passive instrumentation 完成，tag `cache-inst-v0`，分支 `hrl/cache-instrumentation-v0`
- [x] **Round Q**：Paper reproduction & idea development workflow 基础设施建立，分支 `hrl/repro-infra-v0`
- [x] **Round R**：选择 CCWS 作为第一篇 cache 论文；论文深度阅读；写 repro plan；源码 mapping
- [x] **Round S**：创建 `hrl/paper/ccws-repro-v0`；audit `swl_scheduler`（`ccws_swl_audit.md`）；加 CCWS config knobs（9 个，全 default 0）；`paper_ccws_*` no-op stats；编译 pass；feature_off ≈ baseline 4 workload 验证 ✓；tag `ccws-config-noop` 打好
- [x] **Round T**：no-op behavior check；创建 `hrl-repro` config 副本（off/on_noop）；`GPGPUSIM_CONFIG_OVERRIDE` env var 支持加入 `run_one.sh`；quick set 7/7 通过（off + on_noop 两组）；`paper_ccws_enabled` 区分验证 ✓；behavior 计数器全 0 ✓
- [x] **Round U**：SWL static baseline；基于已有 `swl_scheduler` / `warp_limiting` 创建 limit_4/8/16 hrl-repro config 副本；quick set 3×7=21 workload 全部通过；**发现 quick workload 太小**，所有 limit 结果完全相同（<1 warp/scheduler），差异来自 LRR→GTO，非 warp limiting；`paper_ccws_*` 全 0 ✓；不修改 src/
- [x] **Round V**：VTA miss-side probe 实现；`evicted_block_info` 无 warp_id 确认 → miss-side 近似（per-warp 环形 buffer）；新 knob `gpgpu_ccws_enable_vta_probe`（default 0）；probe point: `L1_latency_queue_cycle()` MISS branch；quick set 7/7 pass：`sim_cycle` 不变，`load_gate_block=0`，`vta_probe/hit>0` 对所有 L1D miss workload ✓；VTA hit rate 9–75% 与访存模式一致
- [x] **Round W**：Stage S5 完成 — per-warp LLS 数组加入 `ldst_unit`；6 个新 config knob（`enable_lls_score/base_score/hit_increment/decay_interval/decay_amount/max_score`）；VTA hit → `ccws_lls_update(wid)`；per-cycle score decay（in `ldst_unit::cycle()`）；quick set 7/7 pass：`sim_cycle` 不变，`lls_score_update = vta_hit`（完全相等），`load_gate_block=0`；`atomic_contention` lls_update=0 ✓；`mutual_tiled` 最终 nonzero_warps=0（decay 平衡 hits）✓；无 Can Issue gating
- [x] **Round X**：Would-gate telemetry 完成 — sort+prefix-sum 计算 `m_ccws_would_can_issue[]`（per cycle in `ldst_unit`）；scheduler 调用 `ccws_wg_check_load()` 对每个 LOAD_OP 尝试计数（不阻塞）；3 个新 knob（`enable_would_gate`, `wg_k_throttle`, `wg_debug`）；quick set 7/7 pass：`sim_cycle` 不变，`load_gate_block=0`，`would_gate_attempt>0` 对所有 workload，`would_gate_block=2` for `rodinia_hotspot` ✓
- [x] **Round Y**：Stage S6+S7 完成 — 真实 load-only gating；`ccws_lg_gate_load(wid)` 查询 `would_can_issue[wid]`，阻塞 LOAD_OP / TENSOR_CORE_LOAD_OP；2 个新 knob（`enable_load_gating`, `load_gate_debug`）；feature_off 7/7 pass（cycle=baseline，所有计数器=0）；load_gate_on 7/7 pass：`rodinia_hotspot` `lg_block=5`（真实 gating 生效），`lg_block=wg_block` ✓；STORE / compute 不受影响 ✓
- [x] **Round Z**：Post-gating validation — 7 workload × 3 threshold（default/conservative/aggressive）；feature_off 7/7 pass；load_gate_on 7/7 pass；只有 `rodinia_hotspot` 出现 `lg_block=5`；threshold sweep 无效（`base_score` 同时控制初始值和 cutoff，不是独立 threshold）；高 vta_hit workload（srad_v2/fdtd2d）无 gating（hits 分散）；sim_cycle 未变化（5 blocks 太少）；关键发现：需要独立 `lls_gate_threshold` knob
- [x] **Round AA**：新增独立 knob `gpgpu_ccws_lg_score_threshold`（default 100）；`cum_cutoff = nw * lg_score_threshold`（不再用 `lls_base_score`）；tiny validation 3 workload × 4 threshold：th99/100 → hotspot `lg_block=5`，th101/200 → 0 blocks；threshold 有效 ✓；注意：threshold < base_score 会 deadlock（已删除 th50 config）；有效范围 `lg_score_threshold >= lls_base_score`
- [x] **Round AB**：Focused threshold validation — 7 workload × th99/100/101；28 runs 全部 pass；只有 `rodinia_hotspot` 出现 `lg_block=5`（th99/100），th101 → 0 blocks；趋势单调正确 ✓；sim_cycle 未变化；信号弱（tiny workload + `lls_hit_increment=1`）；建议 standard validation 前先增大 `lls_hit_increment`（10–50）
- [x] **Round AC**：LLS hit-increment sensitivity — 7 workload × inc1/10/50（th100）+ inc10（th101）；inc=1：只 hotspot 5 blocks（弱）；inc=10：全部 0 blocks（timing 问题）；inc=50：srad_v2 +45% cycle / fdtd2d +61% cycle（过度 gating）；建议 standard validation 前先试 inc=5 或 inc=20
- [x] **Round AD**：Hit-increment calibration — 7 workload × inc5/20/30（th100）；全部 0 blocks；根本原因确认：gate 只在 warp 有 LOAD_OP 准备发射时触发，高 LLS warp 已被 scoreboard stall，无法到达 issue 阶段；inc=50 通过正反馈绕过此限制但过度 gating
- [x] **Round AE**：Gating insertion point audit — 确认当前 gate 在 `checkCollision()` 之后（post-scoreboard）；高 LLS stall warp 永远不到达 gate；推荐方向 B1：将 gate 移到 `checkCollision()` 之前（pre-scoreboard），约 10 行改动；不改变 non-load 指令行为
- [x] **Round AF**：Pre-scoreboard gate 实现 — 将 `ccws_lg_gate_load()` 移到 `checkCollision()` 之前；feature_off 7/7 pass；inc=5/20/30 仍全 0 blocks；根本原因修正：VTA hit 分散在太多 warp，每个 warp 的 LLS 分数远低于 cutoff/nw（6400/64=100）；inc=50 行为与 post-scoreboard 完全相同（srad_v2 +45%，fdtd2d +61%）；gate 位置不是问题所在
- [x] **Round AG**：Can-Issue cutoff 审计 — 确认主要 bug：`nw = max_warps_per_shader = 64`，而实际 active warps ≈ 8（occupancy 12%）；`cum_cutoff = 64×100 = 6400`，正确值应为 `8×100 = 800`；inactive warp 的 base_score(100) 合计 5600，消耗 cutoff budget 的 87.5%；`would_can_issue=false` 只落在 inactive warp slot 上，这些 warp 不发射 load，gate 永远不触发；修复方案：用 `not_completed/warp_size` 替换 `max_warps_per_shader` 作为 nw
- [ ] **Round AH**：修复 cutoff 计算 — 用 active warp 数替换 max_warps；只遍历 active warp slot；feature_off 7/7 pass；inc=5/20/30 预期出现 lg_block > 0
- [ ] **Round（后置）**：选择下一步方向（A: inc=50+高 threshold；B: 更早 pipeline 插入点；C: 接受当前行为进入 standard validation）；至少一篇论文 standard_pass 后，开 `hrl/idea/cache-policy-experiments-v0`

## Workload Management Framework（Round B 新增）

目录：`/workspace/repos/gpgpu-workloads/`

```
micro/        自写微基准 (Tier 0)
suites/       Rodinia（已下载）/ PolyBench-GPU（已下载）/ Parboil / SHOC（未下载）
suites/rodinia-wrapper/   Rodinia tiny build/run 目录（wrapper 模式，不污染原始源码）
runs/         每次运行输出（logs + stats + provenance）
scripts/      运行脚本和统计提取脚本
manifests/    workload 注册表（CSV）
docs/         说明文档
```

### 关键脚本

```bash
# 列出所有 workload
bash /workspace/repos/gpgpu-workloads/scripts/list_workloads.sh
bash /workspace/repos/gpgpu-workloads/scripts/list_workloads.sh --status ready
bash /workspace/repos/gpgpu-workloads/scripts/list_workloads.sh --tier Tier0

# 运行单个 workload（自动 tee log + 提取 stats + 写 provenance）
bash /workspace/repos/gpgpu-workloads/scripts/run_one.sh vecadd

# 提取单个 log 统计（增强版，含 L1D / stall / network / TLB 预留字段）
python3 /workspace/repos/gpgpu-workloads/scripts/extract_gpgpusim_stats.py <log_file>
python3 /workspace/repos/gpgpu-workloads/scripts/extract_gpgpusim_stats.py <log> \
    --csv results.csv --meta "app=vecadd,commit=abc123"

# 汇总所有 workload 最新结果（Round E 新增）
python3 /workspace/repos/gpgpu-workloads/scripts/summarize_runs.py
python3 /workspace/repos/gpgpu-workloads/scripts/summarize_runs.py --csv > runs/latest_summary.csv
python3 /workspace/repos/gpgpu-workloads/scripts/summarize_runs.py --workloads vecadd,strided_access
```

### Round D micro workloads（2026-04-30 新增，全部 PASS）

| Workload | 源码 | 默认参数 | 用途 |
|----------|------|---------|------|
| `mutual_naive` | `micro/mutual_naive/mutual_naive.cu` | N=32 blockDim=16×16 | 全 global-memory matmul，无复用 |
| `mutual_tiled` | `micro/mutual_tiled/mutual_tiled.cu` | N=32 TILE=16 | shared memory tiling，降低 global traffic |

**GPGPU-Sim 运行结果（Round D）：**

| Workload | sim_cycle | IPC | L2 accesses | L2 miss | averagemflatency |
|----------|-----------|-----|-------------|---------|-----------------|
| mutual_naive | 12322 | 17.20 | 640 | 20% | 189 |
| mutual_tiled | **7479** | **25.74** | 640 | 20% | 200 |

mutual_tiled 比 mutual_naive 快 **39%**（IPC 高 50%）。L2 accesses 相同（N=32 全矩阵 4KB 可全驻 L2），区别在于 tiled 通过 shared memory 完全消除了 L1D→L2 的重复请求，使 warp 等待减少。

### Round E 统计增强（2026-04-30）

新增脚本：`scripts/summarize_runs.py`，扫描 `runs/` 最新 log 生成对比表；`runs/latest_summary.csv` 为最新汇总结果。

`extract_gpgpusim_stats.py` 新增提取字段：

| 类别 | 字段 |
|------|------|
| Occupancy | `gpu_tot_occupancy_pct` |
| ICNT latency | `max_icnt2mem_latency`, `max_icnt2sh_latency` |
| L2 extra | `L2_total_cache_pending_hits`, `L2_total_cache_reservation_fails` |
| L1D core[0] | `l1d_c0_accesses/misses/miss_rate/pending_hits/reservation_fails` |
| L1D aggregate | `l1d_agg_accesses/misses/reservation_fails` |
| Stall | `ws_Stall`, `ws_W0_Idle`, `ws_W0_Scoreboard` |
| Network | `Req/Reply_Network_injected_packets_num/per_cycle` |
| TLB 预留 | `gpgpu_tlb_accesses/hits/misses/miss_rate/total_latency_cycles/n_tlb_stall`（全 NA，等待实现） |

**6 个 Tier0 最新统计快照（来自 `runs/latest_summary.csv`）：**

| Workload | sim_cycle | IPC | L1D_miss(c0) | L2_miss | avgmfl | W0_Scoreboard |
|----------|-----------|-----|-------------|---------|--------|--------------|
| vecadd | 5569 | 0.97 | 1.000 | 33% | 211 | 1154 |
| strided_access | 5825 | 0.84 | 0.222 | 50% | 187 | 2215 |
| page_stride_access | 5851 | 0.83 | 0.333 | 33% | 187 | 2094 |
| atomic_contention | 5414 | 0.66 | -nan\* | 0% | NA | 692 |
| mutual_naive | 12322 | 17.2 | 0.152 | 20% | 189 | 106105 |
| mutual_tiled | 7479 | 25.7 | 1.000 | 20% | 200 | 28777 |

\* atomic_contention L1D access=0（atomic 不走 L1D read 路径）

### Round C micro workloads（2026-04-30 新增，全部 PASS）

| Workload | 源码 | 默认参数 | 用途 |
|----------|------|---------|------|
| `strided_access` | `micro/strided_access/strided_access.cu` | n=1024 stride=32 threads=256 | coalescing 退化、L2 miss 升高 |
| `page_stride_access` | `micro/page_stride_access/page_stride_access.cu` | 64 pages × 4KB threads=256 | TLB page footprint 最大化 |
| `atomic_contention` | `micro/atomic_contention/atomic_contention.cu` | n=256 num_bins=16 threads=256 | atomic write serialization |

**GPGPU-Sim 运行结果对比（vs vecadd baseline）：**

| Workload | sim_cycle | IPC | L2 miss | averagemflatency |
|----------|-----------|-----|---------|-----------------|
| vecadd | 5569 | 0.9653 | 33% | 211 |
| strided_access | 5825 | 0.8350 | **50%** | 187 |
| page_stride_access | 5851 | 0.8313 | 33% | 187 |
| atomic_contention | 5414 | **0.6620** | 0% | NA (atomic) |

**关键编译坑**：所有 micro 必须加 `-cudart shared`，否则 nvcc 静态链接 libcudart，GPGPU-Sim 无法通过 LD_LIBRARY_PATH 拦截 CUDA 调用，模拟不会发生（程序返回 FAIL）。

```makefile
CFLAGS = -O2 -arch=sm_52 -cudart shared --ptxas-options=-v
```

所有 micro 配置文件已复制到各自目录（`gpgpusim.config` + `config_volta_islip.icnt`），run_one.sh 可直接使用。

### Round F PolyBench-GPU 带入（2026-04-30 新增，全部 gpgpusim_exit=1）

**源码位置**：`/workspace/repos/gpgpu-workloads/suites/polybench-gpu/`（官方 git clone）

**Build 目录**：`/workspace/repos/gpgpu-workloads/polybench/pb_{gemm,atax,2dconv,fdtd2d}/`

每个目录包含：
- 复制并 patch 的 `.cu` / `.cuh`（`../../common/` → `../`，size 宏直接硬编码为小值）
- `Makefile`（`-O2 -arch=sm_52 -cudart shared --ptxas-options=-v`）
- `gpgpusim.config` + `config_volta_islip.icnt`（SM7_QV100）

**关键 build 坑**：
1. 原始 `.cuh` 的 size 宏对所有 dataset 类型都是 4096/2048，必须直接改 .cuh；用 `-DN=1` 绕不开是因为 `cuda.h` 里有 `size_t N` 函数参数
2. polybench.c 已被 `.cu` 通过 `#include` 内联，不能再作为独立编译单元（会产生 multiple definition 链接错误）

**GPGPU-Sim 运行结果（4 benchmarks，全部 gpgpusim_exit=1）：**

| Workload | size | sim_cycle | IPC | L1D_miss(c0) | L2_miss | W0_Scoreboard |
|----------|------|-----------|-----|-------------|---------|--------------|
| polybench_gemm | 64×64×64 | 24543 | 80.3 | 0.075 | 0% | 1153453 |
| polybench_atax | 128×128 | 77442 | 19.4 | 0.014 | 0% | 331047 |
| polybench_2dconv | 128×128 | 6652 | 123.5 | 0.243 | 37% | 211260 |
| polybench_fdtd2d | 64×64 TMAX=2 | 35681 | 24.6 | 0.546 | 0% | 33248 |

注：`result_pass=0` 是正常的——PolyBench 输出 CPU-GPU 精度比对结果，没有 "PASS" 字符串；只要 `gpgpusim_exit=1` 就说明模拟完整跑完。

### Manifest 条目（Round G 状态）

| suite | app | tier | status |
|-------|-----|------|--------|
| existing | vecadd | Tier0 | **ready** |
| micro | mutual_naive / mutual_tiled / strided_access / page_stride_access / atomic_contention | Tier0 | **ready** |
| polybench | polybench_gemm / polybench_atax / polybench_2dconv / polybench_fdtd2d | Tier1 | **ready** |
| rodinia | rodinia_pathfinder / rodinia_hotspot / rodinia_srad_v2 / rodinia_lud | Tier1 | **ready** |
| planned | parboil_spmv / parboil_stencil / parboil_sgemm | Tier2 | planned |

### Round G Rodinia 带入（2026-04-30 新增，全部 gpgpusim_exit=1）

**Rodinia 来源**：`git clone --depth=1 https://github.com/yuhc/gpu-rodinia.git suites/rodinia/`

**Wrapper 目录**：`/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/`（每个 app 独立子目录，含复制的源码、Makefile、SM7_QV100 config）

每个 wrapper 包含：
- 复制的 `.cu` / `.h` 源码（原始 Rodinia 只读，不污染）
- `Makefile`（`-O2 -arch=sm_52 -cudart shared --ptxas-options=-v`）
- `gpgpusim.config` + `config_volta_islip.icnt`（SM7_QV100）

**关键 build 坑**：
1. hotspot 需要 temp/power 输入文件；用 Python 生成 64×64 tiny 版（`temp_64`, `power_64`）
2. lud 原始 Makefile 分开编译 `lud_kernel.cu`，直接 `nvcc lud.cu common.c` 报 `undefined reference to lud_cuda()`；必须在命令行同时传入 `lud_kernel.cu common.c`

**GPGPU-Sim 运行结果（4 benchmarks，全部 gpgpusim_exit=1）：**

| Workload | tiny 参数 | sim_cycle | IPC | L1D_miss(c0) | L2_miss | avgmfl | W0_Scoreboard |
|----------|----------|-----------|-----|-------------|---------|--------|--------------|
| rodinia_pathfinder | cols=64 rows=64 pyramid=2 | 207,186 | 3.74 | 0.800 | 0.7% | 189 | 3,278 |
| rodinia_hotspot | 64×64 1iter | 6,931 | 133.4 | 1.000 | 19.9% | 208 | 122,596 |
| rodinia_srad_v2 | 64×64 1iter | 15,926 | 69.3 | 0.792 | 34.2% | 200 | 134,685 |
| rodinia_lud | -s 32 -v | 97,518 | 1.09 | 0.516 | 0.0% | 188 | 19,562 |

**行为特征说明**：
- `hotspot` / `srad_v2`：memory-bound stencil，L1D miss 率高，W0_Scoreboard 主导；适合 TLB latency 实验
- `pathfinder`：dynamic programming stencil，共享内存密集，207K cycles 最长但 L2 miss 极低（0.7%）
- `lud`：tiled dense LU，32×32 时 occupancy 仅 1.77%；建议后续改为 `-s 64` 或 `-s 128`

注：`result_pass=0` 正常——Rodinia 无通用 "PASS" 字符串；`gpgpusim_exit=1` 确认模拟完整跑完。

## Known limitations

| Issue | Detail |
|-------|--------|
| No real GPU | `nvidia-smi` unavailable in container |
| PTX simulation only | No real SASS execution |
| No OpenCL | Driver not installed |
| Slow simulation | ~203268x slower than real hardware — keep kernels small |
| No cuDNN/cuBLAS | Not installed |
| Env not persistent | Re-source `load_gpgpusim.sh` in each new shell (CUDA path is in `~/.bashrc`) |

## 全量 14 Workload 统计快照（Round G 完成后，来自 `runs/latest_summary.csv`）

| Workload | sim_cycle | IPC | L1D_miss(c0) | L2_miss | avgmfl | W0_Scoreboard |
|----------|-----------|-----|-------------|---------|--------|--------------|
| vecadd | 5,569 | 0.97 | 1.000 | 33% | 211 | 1,154 |
| strided_access | 5,825 | 0.84 | 0.222 | 50% | 187 | 2,215 |
| page_stride_access | 5,851 | 0.83 | 0.333 | 33% | 187 | 2,094 |
| atomic_contention | 5,414 | 0.66 | -nan | 0% | NA | 692 |
| mutual_naive | 12,322 | 17.2 | 0.152 | 20% | 189 | 106,105 |
| mutual_tiled | 7,479 | 25.7 | 1.000 | 20% | 200 | 28,777 |
| polybench_gemm | 24,543 | 80.3 | 0.075 | 0% | 199 | 1,153,453 |
| polybench_atax | 77,442 | 19.4 | 0.014 | 0% | 195 | 331,047 |
| polybench_2dconv | 6,652 | 123.5 | 0.243 | 37% | 209 | 211,260 |
| polybench_fdtd2d | 35,681 | 24.6 | 0.546 | 0% | 194 | 33,248 |
| rodinia_hotspot | 6,931 | 133.4 | 1.000 | 20% | 208 | 122,596 |
| rodinia_srad_v2 | 15,926 | 69.3 | 0.792 | 34% | 200 | 134,685 |
| rodinia_lud | 97,518 | 1.09 | 0.516 | 0% | 188 | 19,562 |
| rodinia_pathfinder | 207,186 | 3.74 | 0.800 | 1% | 189 | 3,278 |

## Round Q: Paper Reproduction & Idea Development Workflow（2026-04-30）

### 本轮目标

建立论文复现与自研机制开发的工作流基础设施。**不修改 simulator 行为，不做 cache policy 实验，不运行 workload。**

### 当前分支体系

| 分支 / Tag | 说明 |
|-----------|------|
| `baseline-a4ce3fe` | 上游 baseline 锚点（已存在） |
| `cache-inst-v0` | Cache instrumentation 稳定 tag（已存在） |
| `hrl/cache-instrumentation-v0` | Cache instrumentation 分支 |
| `hrl/repro-infra-v0` | **当前分支**，论文复现框架文档 |
| `hrl/paper/<paper-key>-repro-v0` | 每篇论文独立分支（待创建） |
| `hrl/idea/<idea-key>-v0` | 每个自研机制独立分支（待创建） |
| `hrl/integration/cache-papers-v0` | 多篇 paper 合并验证（待创建） |
| `hrl/integration/cache-final-v0` | 最终组合（待创建） |

### 核心规则（恢复上下文用）

1. **每篇论文一个 paper branch**，忠实复现，不混入自研创新。
2. **每个自研机制一个 idea branch**，不寄生在 paper branch。
3. **paper / idea 分支通过 integration 分支选择性合并**，互不直接 merge。
4. **行为改动必须 feature flag default off**：`-gpgpu_enable_<key> 0`。
5. **三组验证必须做**：baseline / feature_off / feature_on；`feature_off ≈ baseline` 是第一成功标准。
6. **Workload sets**：quick（7，smoke）/ standard（13，正式实验）/ extended（全量，最终确认）。
7. **方向顺序**：先 cache，后 TLB/MMU。
8. **自研实验后置**：至少一篇论文 standard_pass 后再开 `hrl/idea/cache-policy-experiments-v0`。

### 本轮新增文件

| 文件 | 说明 |
|------|------|
| `docs/reproduction_workflow.md` | 完整工作流文档（分支策略、tag、worktree、验证流程） |
| `docs/paper_repro_template.md` | 论文复现计划模板（每篇论文 copy 填写） |
| `docs/idea_branch_template.md` | 自研机制分支模板 |
| `experiments/README.md` | 实验元数据目录说明和 result_manifest.csv 格式 |
| `configs/hrl-repro/README.md` | Config 复制规范，不修改 tested-cfgs |
| `docs/papers/README.md` | 论文复现进度表和 stage 定义 |
| `docs/ideas/README.md` | 自研 idea 进度表和 stage 定义 |

### 下一步：Round R

选择第一篇 cache 论文，填写 `docs/papers/<paper-key>_repro_plan.md`，**不改代码**。

候选方向：CCWS / DAWS / PCAL / Linebacker / RRIP。

## Round R: CCWS Paper Reproduction Plan（2026-04-30）

### 本轮目标

选择 CCWS（Cache-Conscious Wavefront Scheduling，Rogers/O'Connor/Aamodt，MICRO 2012）作为第一篇 cache 论文。深度阅读论文，映射到 GPGPU-Sim 源码，写 repro plan。**不修改 src/，不运行 workload，不编译。**

### 关键发现（恢复上下文用）

| 发现 | 细节 |
|------|------|
| `swl_scheduler` 已存在 | `shader.cc:1678`，config key `warp_limiting:<prio>:<limit>`，目前只支持 GTO，是 SWL 直接实现 |
| LOAD_OP gating 候选点 | `shader.cc:1344` 的 `if ((pI->op == LOAD_OP)||...)` 判断处 |
| `issue_warp` 调用前插入点 | `shader.cc:1352`，`warp_id` 在作用域内 |
| `evicted_block_info` 无 warp_id | `gpu-cache.h:82`，faithful VTA 需要修改此结构，是最高风险点 |
| `mf->get_wid()` 可用 | `mem_fetch.h:98`，miss 时可获取 warp_id |
| SM7_QV100 当前 scheduler | `lrr`（config line 134），不是 gto |
| CCWS K_THROTTLE 最优值 | `8`（单一静态值，对所有 HCS workload 有效） |

### 本轮新增文件

| 文件 | 说明 |
|------|------|
| `docs/papers/ccws_reading_notes.md` | 论文深度阅读笔记 |
| `docs/papers/ccws_repro_plan.md` | CCWS 复现计划（从模板填写）|
| `experiments/paper-ccws/README.md` | 实验元数据目录说明 |
| `experiments/paper-ccws/config_matrix.csv` | Config 矩阵（baseline/off/on） |
| `experiments/paper-ccws/result_manifest.csv` | 结果 manifest 表头 |

### Round S 建议路线

1. 创建 `hrl/paper/ccws-repro-v0`（base: `hrl/repro-infra-v0`）
2. **Stage S1**：审计 `swl_scheduler`，确认语义与论文 SWL 一致
3. **Stage S2**：只加 config knobs + no-op feature flag，编译通过
4. **Stage S3**：feature_off quick pass，确认 ≈ baseline
5. **Stage S4+**：渐进实现 instrumentation → VTA/LLD → load gating
6. 不立即全量实现完整 CCWS

### 自研扩展规则

CCWS paper branch **只做忠实复现**。任何对 CCWS 的自研改进或扩展必须放到 `hrl/idea/<idea-key>-from-ccws-v0`，不得混入 paper branch。

## Round S: CCWS No-op Config + feature_off Pass（2026-04-30）

### 完成内容

| Stage | 内容 | 结果 |
|-------|------|------|
| S1 | audit `swl_scheduler`/`warp_limiting` | `ccws_swl_audit.md` 写好；结论：与论文 SWL 语义一致，无需修改，reuse 即可 |
| S2 | 添加 9 个 CCWS config knobs（全 default 0） | `shader.h` + `gpu-sim.cc` 各加约 40 行；编译 pass |
| S3 | feature_off quick pass | 4 workload sim_cycle 全部 = baseline；`paper_ccws_load_gate_block = 0` |
| S4 | `paper_ccws_*` no-op stats | 9 行 `fprintf` 输出 key=value 格式；全 zero 时 feature_off pass |

### 修改的源文件

| 文件 | 修改内容 | 位置 |
|------|---------|------|
| `src/gpgpu-sim/shader.h` | 在 `shader_core_config` 末尾（原 line 1719 后）加 9 个 `gpgpu_ccws_*` 成员变量 | 紧接 `m_specialized_unit_num` 后 |
| `src/gpgpu-sim/gpu-sim.cc` | 在 `shader_core_config::reg_options()` 末尾加 9 个 `option_parser_register` 调用 | 原 line 655–664 的 for 循环之后 |
| `src/gpgpu-sim/gpu-sim.cc` | 在 `print_stats()` 的 `print_cacheinst_stats(stdout, l2_stats, "L2")` 之后加 9 行 `paper_ccws_*` 输出 | 原 line 1602 之后 |

### 新增文档文件

| 文件 | 内容 |
|------|------|
| `docs/papers/ccws_swl_audit.md` | SWL audit 结论：`swl_scheduler` 语义、源码走读、与论文 SWL 对比 |
| `docs/papers/ccws_round_s_noop_config.md` | Round S config knobs 完整列表，feature_off 验证结果表 |

### Tag 待打

```bash
git tag ccws-config-noop   # 在 commit 后打（Round S 提交打的）
```

### 下一步：Round S+ (Stage S5)

- 在 `scheduler_unit` 中添加 per-warp LLS 数组（`unsigned ccws_lls[MAX_WARPS_PER_CTA]`）
- 添加 miss-side VTA 原型（`mf->get_wid()` 在 L1D MISS 时记录到 per-scheduler VTA）
- LLS score decay（per-cycle -1，floor = BaseScore）
- LLS score update（VTA hit → jump to LLDS）
- 此时 `paper_ccws_vta_hit > 0` 应在 HCS workload（如 `page_stride_access`）上出现

## Round T: CCWS No-op Feature Behavior Check（2026-04-30）

### 本轮目标

验证 CCWS no-op 阶段（Round S 添加的 config knobs + stats）不改变行为：
- `feature_off`（`gpgpu_enable_ccws=0`）≈ baseline  
- `feature_on_noop`（`gpgpu_enable_ccws=1`，无 VTA/LLS/gating）≈ feature_off

### 完成内容

| 内容 | 结果 |
|------|------|
| 创建 `configs/hrl-repro/SM7_QV100_ccws_noop_off/` | ✓ SM7_QV100 + `-gpgpu_enable_ccws 0` |
| 创建 `configs/hrl-repro/SM7_QV100_ccws_noop_on/` | ✓ SM7_QV100 + `-gpgpu_enable_ccws 1` |
| `GPGPUSIM_CONFIG_OVERRIDE` env var 加入 `run_one.sh` | ✓ 6行改动，复制 config 到 exec dir |
| feature_off quick set 7/7 | ✓ failed=0；sim_cycle 全匹配 baseline |
| feature_on_noop quick set 7/7 | ✓ failed=0；sim_cycle 全匹配 feature_off |
| `paper_ccws_enabled` 区分 0/1 | ✓ |
| 行为计数器全 0 | ✓ vta_hit=0, load_gate_block=0, lost_locality=0 |

### feature_off / feature_on_noop sim_cycle 比较（7 workloads）

| Workload | feature_off | feature_on_noop | Match |
|----------|-------------|-----------------|-------|
| vecadd | 5569 | 5569 | ✓ |
| strided_access | 5825 | 5825 | ✓ |
| page_stride_access | 5851 | 5851 | ✓ |
| atomic_contention | 5414 | 5414 | ✓ |
| mutual_tiled | 7479 | 7479 | ✓ |
| polybench_2dconv | 6652 | 6652 | ✓ |
| rodinia_hotspot | 6931 | 6931 | ✓ |

### 新增/修改文件

| 文件 | 类型 |
|------|------|
| `configs/hrl-repro/SM7_QV100_ccws_noop_off/` | 新增（GPGPU-Sim 仓库） |
| `configs/hrl-repro/SM7_QV100_ccws_noop_on/` | 新增（GPGPU-Sim 仓库） |
| `experiments/paper-ccws/config_matrix.csv` | 更新（加 2 行） |
| `experiments/paper-ccws/noop_behavior_check.csv` | 新增（运行结果） |
| `docs/papers/ccws_round_t_noop_behavior_check.md` | 新增（behavior check 报告） |
| `docs/papers/ccws_repro_plan.md` | 更新（Round T 状态） |
| `/workspace/repos/gpgpu-workloads/scripts/run_one.sh` | 更新（config override） |

### workload 仓库注意

- `runs/latest_summary.csv` 有改动：**不建议提交**（每次运行都会改，属于本地输出）
- `scripts/run_one.sh` 有改动：建议单独提交到 workload 仓库

### 下一步：Round U

优先方案（先选一个）：
1. **SWL controllable baseline**：wire `gpgpu_ccws_enable_swl` 到 `swl_scheduler`，实现可控的 SWL 对照组
2. **Stage S5 VTA 原型**：miss-side VTA + LLS array + score decay；`paper_ccws_vta_hit > 0` 应在 HCS 上出现

**严格规则**：自研 cache policy 不得进入 `hrl/paper/ccws-repro-v0`。

## Round U: CCWS SWL Static Baseline（2026-05-01）

### 本轮目标

将现有 `swl_scheduler` / `warp_limiting` 纳入复现流程，建立可重复的 SWL 对照组。**不修改 src/。**

### 完成内容

| 内容 | 结果 |
|------|------|
| 确认 SWL 语义 | `warp_limiting:2:<limit>`；GTO-only（assert）；每 scheduler 限制 prioritized warp 数 |
| `SM7_QV100_ccws_swl_limit_4` | ✓ 创建；`warp_limiting:2:4` |
| `SM7_QV100_ccws_swl_limit_8` | ✓ 创建；`warp_limiting:2:8` |
| `SM7_QV100_ccws_swl_limit_16` | ✓ 创建；`warp_limiting:2:16`（= GTO reference，无实际 limit） |
| quick set 3×7 workloads | ✓ 21 runs，failed=0 |
| src/ 修改 | **无** |
| 编译 | 无需（配置改动） |

### 关键发现：quick workload 太小

SM7_QV100 每 scheduler 约 16 warps。**quick set 的 tiny workloads 每 scheduler 只有 <1 warp 激活**，所以 limit_4/8/16 结果完全相同。差异（0.1–0.4%）来自 LRR→GTO 调度策略，非 warp limiting 效果。

需要更大 workload（cache_focus / irregular_focus set）才能观察 SWL 效果。

### sim_cycle 摘要（7 quick workloads）

| Workload | LRR baseline | SWL 任意 limit | 差异来源 |
|----------|-------------|---------------|---------|
| vecadd | 5569 | 5558 | GTO vs LRR |
| strided_access | 5825 | 5824 | GTO vs LRR |
| page_stride_access | 5851 | 5850 | GTO vs LRR |
| atomic_contention | 5414 | 5407 | GTO vs LRR |
| mutual_tiled | 7479 | 7451 | GTO vs LRR |
| polybench_2dconv | 6652 | 6660 | GTO vs LRR |
| rodinia_hotspot | 6931 | 6917 | GTO vs LRR |

### 新增文件（本轮，待 commit）

| 文件 | 内容 |
|------|------|
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_4/` | SWL config（limit=4） |
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_8/` | SWL config（limit=8） |
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_16/` | SWL config（limit=16 / GTO ref） |
| `experiments/paper-ccws/swl_baseline_check.csv` | 3 limit × 7 wl 运行结果 |
| `experiments/paper-ccws/config_matrix.csv` | 更新（+3 行） |
| `docs/papers/ccws_round_u_swl_baseline.md` | Round U 文档 |
| `docs/papers/ccws_repro_plan.md` | Stage 状态更新 |
| `CLAUDE.md` | Round U 摘要 |

### workload 仓库

- `runs/latest_summary.csv`：有改动，**不建议提交**
- 其他文件：无改动（workload 脚本 `run_one.sh` 已在 Round T 提交）

### 下一步：Round W（Stage S5）

在 `scheduler_unit` 加 per-warp LLS 数组；per-scheduler `VTAHitsTotal`/`InstIssuedTotal` 计数；VTA hit → LLDS 更新；per-cycle score decay；先验证 `score_update/decay > 0`，no gating。

**严格规则**：自研 cache policy 不得进入 `hrl/paper/ccws-repro-v0`。

---

## Round V: CCWS VTA Probe Instrumentation（2026-05-01）

### 本轮目标

在 `ldst_unit` 中插入 VTA-like miss-side probe，作为 **pure instrumentation**（纯计数，无调度行为改变）。验证 L1D miss 路径可拿到 warp_id 和 block_addr，使 `paper_ccws_vta_probe/hit > 0`，同时保持 `sim_cycle` 与 baseline 完全一致。

### 关键约束（本轮严格遵守）

| 约束 | 状态 |
|------|------|
| 无 LLS / score array | ✓ |
| 无 score decay | ✓ |
| 无 Can Issue gating | ✓ |
| 无 load 阻塞 | ✓ (`load_gate_block = 0` 所有 workload) |
| 无调度行为变化 | ✓ (`sim_cycle` 全部 = baseline) |
| 使用 miss-side 近似，不是 faithful eviction-based VTA | ✓ |

### VTA 近似方案说明

`evicted_block_info`（`gpu-cache.h:82`）无 warp_id 字段，因此**无法使用 eviction-based VTA**。本轮使用 **miss-side approximation（方案 B）**：

- 每个 warp 维护一个大小为 `vta_entries_per_warp`（默认 16）的环形 buffer，存储最近 miss 的 block 地址
- 每次 L1D MISS：probe VTA[wid] for block_addr → 若找到则 `vta_hit++`（代表"此 warp 重复 miss 同一 block"，近似代替"lost locality"）
- 然后将 block_addr 插入 VTA[wid]（替换最旧 slot）

**与论文 VTA 的区别**：论文 VTA 记录被 *其他 warp 驱逐* 的该 warp 的 cache line owner，用于检测 inter-warp eviction。本轮的近似记录的是 warp 自己的重复 miss，是 intra-warp locality 的代理指标。足够用于验证机制，Stage S6+ 可视需要替换为精确版本。

### 新增 Config Knob

| Knob | Default | 说明 |
|------|---------|------|
| `-gpgpu_ccws_enable_vta_probe` | `0` | 启用 miss-side VTA probe（不影响调度） |

当 `gpgpu_enable_ccws=0` 时，`ccws_vta_probe_miss()` 完全不执行。

### 新增 Stats

| Stat | 说明 |
|------|------|
| `paper_ccws_l1d_miss_seen` | 总 L1D miss 数（gpgpu_enable_ccws=1 时统计） |
| `paper_ccws_vta_probe` | 执行了 VTA probe 的 miss 数 |
| `paper_ccws_vta_hit` | VTA probe hit（same block before by this warp） |
| `paper_ccws_vta_insert` | VTA 插入次数 |
| `paper_ccws_vta_overwrite` | 插入时覆盖已有 entry 的次数（VTA 满后） |

### 修改的源文件

| 文件 | 修改内容 |
|------|---------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`：VTA state + 计数器 + `ccws_vta_probe_miss()` + `get_ccws_vta_stats()`；`shader_core_config`：新增 `gpgpu_ccws_enable_vta_probe`；`shader_core_ctx`、`simt_core_cluster`：新增 `get_ccws_vta_stats()` 声明 |
| `src/gpgpu-sim/shader.cc` | `ldst_unit::init()`：初始化 VTA 表；`L1_latency_queue_cycle()`：MISS branch probe（SM7_QV100 主路径）；`process_cache_access()`：MISS branch probe（fallback）；三处聚合函数实现 |
| `src/gpgpu-sim/gpu-sim.cc` | 注册 `gpgpu_ccws_enable_vta_probe`；`print_stats()`：将硬编码 0 替换为跨 cluster 聚合的真实计数 |

### Quick Set 验证结果（7 workload）

**probe_off（feature_off）**：所有 7 workload sim_cycle = baseline，所有 `paper_ccws_*` = 0。✓

**probe_on（VTA probe 启用）**：

| Workload | sim_cycle | l1d_miss | vta_probe | vta_hit | hit_rate | gate_block |
|----------|-----------|----------|-----------|---------|---------|------------|
| vecadd | 5569 | 96 | 96 | 72 | 75% | 0 |
| strided_access | 5825 | 224 | 224 | 24 | 11% | 0 |
| page_stride_access | 5851 | 256 | 256 | 24 | 9% | 0 |
| atomic_contention | 5414 | 0 | 0 | 0 | — | 0 |
| mutual_tiled | 7479 | 640 | 640 | 384 | 60% | 0 |
| polybench_2dconv | 6652 | 7860 | 7860 | 5616 | 71% | 0 |
| rodinia_hotspot | 6931 | 2576 | 2576 | 1328 | 52% | 0 |

- `atomic_contention`：atomic 不走 L1D read 路径，`l1d_miss_seen=0` 符合预期 ✓
- `strided_access` / `page_stride_access`：stride 破坏 warp 内局部性，hit rate 低（9–11%）符合预期 ✓
- `polybench_2dconv` / `mutual_tiled`：卷积/矩阵数据复用高，hit rate 高（60–75%）符合预期 ✓

### 新增文件

| 文件 | 内容 |
|------|------|
| `configs/hrl-repro/SM7_QV100_ccws_vta_probe_off/` | feature_off config（gpgpu_ccws_enable_vta_probe=0） |
| `configs/hrl-repro/SM7_QV100_ccws_vta_probe_on/` | VTA probe on config（gpgpu_ccws_enable_vta_probe=1） |
| `docs/papers/ccws_round_v_vta_probe.md` | Round V 完整文档 |
| `experiments/paper-ccws/vta_probe_check.csv` | 2×7 workload 运行结果 |

### workload 仓库

无改动。`runs/latest_summary.csv` 有改动，**不建议提交**。

### 下一步：Round Y（Stage S6 — 真正 Can Issue gating）

- 将 `m_ccws_would_can_issue[warp_id]` 检查加入 `scheduler_unit::cycle()` 的 LOAD_OP 分支（`&& m_shader->m_ldst_unit->ccws_would_can_issue(warp_id)`）
- 更新 `paper_ccws_load_gate_block` 统计为真实 gate 计数
- 目标：`paper_ccws_load_gate_block > 0` 在 HCS workload 上出现；`sim_cycle` 对 CI workload 不变

**严格规则**：自研 cache policy 不得进入 `hrl/paper/ccws-repro-v0`。

## Round X: CCWS Would-Gate Telemetry（2026-05-01）

### 本轮目标

实现 CCWS Can-Issue/would-gate 计算逻辑，**纯 telemetry，不阻塞 load，不改变 scheduler/cache/timing 行为。**

### 完成内容

| 内容 | 结果 |
|------|------|
| `m_ccws_would_can_issue[]` 在 `ldst_unit` 中 | ✓ per-warp bool vector，每周期 LLS decay 后 sort+prefix-sum 重计算 |
| `ccws_wg_check_load(wid)` | ✓ scheduler 调用，查 `would_can_issue[wid]`，增 attempt/block/allow |
| 3 wg 计数器 | ✓ `m_ccws_wg_attempt/block/allow` in `ldst_unit` |
| 3 新 config knob | ✓ `enable_would_gate(0)`, `wg_k_throttle(8.0)`, `wg_debug(0)` |
| scheduler_unit::cycle() 6-line telemetry 插入 | ✓ 无 issue_warp 调用修改，不阻塞 load |
| stats 聚合链 | ✓ `ldst_unit → shader_core_ctx → simt_core_cluster → print_stats()` |
| `SM7_QV100_ccws_would_gate_on/` config | ✓ 创建；全套 VTA+LLS+would_gate 开启 |
| quick set 7/7 pass（wg_on） | ✓ `sim_cycle` 不变，`load_gate_block=0`，`would_gate_attempt>0` 全 workload |
| `would_gate_block > 0` | ✓ `rodinia_hotspot` = 2（机制 confirmed functional） |
| feature_off 7/7 pass | ✓ 所有计数器 0 |
| 编译 | ✓ warnings only（pre-existing Wreorder） |

### 关键验证结果

| Workload | vta_hit | wg_attempt | wg_block | gate_block |
|----------|---------|------------|----------|------------|
| vecadd | 72 | 105 | 0 | 0 |
| strided_access | 24 | 97 | 0 | 0 |
| page_stride_access | 24 | 97 | 0 | 0 |
| atomic_contention | 0 | 50 | 0 | 0 |
| mutual_tiled | 384 | 2552 | 0 | 0 |
| polybench_2dconv | 5616 | 10232 | 0 | 0 |
| rodinia_hotspot | 1328 | 16415 | **2** | 0 |

**wg_block = 2 in rodinia_hotspot**：机制 confirmed working。small number 是预期的（quick-set 小 workload，LLS 分数仅微高于 base，cutoff 很少被真正越过）。

### 修改文件（本轮）

| 文件 | 修改内容 |
|------|---------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`: 3 wg state vars + 3 new public methods; `shader_core_config`: 3 new knobs; `shader_core_ctx`/`simt_core_cluster`: declarations |
| `src/gpgpu-sim/shader.cc` | `init()`: wg state init; `cycle()`: sort+prefix-sum after decay; `ccws_wg_check_load()`, `get_ccws_wg_stats()` definitions; `scheduler_unit::cycle()`: 6-line telemetry block; aggregation methods |
| `src/gpgpu-sim/gpu-sim.cc` | Register 3 knobs; add `paper_ccws_would_gate_*` stats block |
| `configs/hrl-repro/SM7_QV100_ccws_would_gate_on/` | 新建 |
| `experiments/paper-ccws/would_gate_check.csv` | 新建（2×7 = 14 runs） |
| `experiments/paper-ccws/config_matrix.csv` | +1 行 |
| `docs/papers/ccws_round_x_would_gate.md` | 新建 |
| `docs/papers/ccws_repro_plan.md` | Stage X ✓, Round X note 加入 |
| `CLAUDE.md` | Round X 摘要 |

### 待提交状态（session 结束时未提交）

```bash
# 建议 commit message:
# ccws: add would-gate telemetry (Round X instrumentation-only)
# 建议 tag: ccws-would-gate-telemetry
git add src/gpgpu-sim/shader.h src/gpgpu-sim/shader.cc src/gpgpu-sim/gpu-sim.cc \
    configs/hrl-repro/SM7_QV100_ccws_would_gate_on/ \
    docs/papers/ccws_round_x_would_gate.md \
    experiments/paper-ccws/would_gate_check.csv \
    experiments/paper-ccws/config_matrix.csv \
    docs/papers/ccws_repro_plan.md CLAUDE.md
git commit -m "ccws: add would-gate telemetry (Round X instrumentation-only)"
git tag ccws-would-gate-telemetry
```

## Round W: CCWS LLS Score Instrumentation（2026-05-01）

### 本轮目标

实现 LLS（Lost-Locality Score）per-warp 数组 + per-cycle decay，纯 instrumentation。**不做 Can Issue gating，不改变调度行为。**

### 完成内容

| 内容 | 结果 |
|------|------|
| `m_ccws_lls` per-warp score vector in `ldst_unit` | ✓ `std::vector<unsigned>`，size = `max_warps_per_shader`，初始化为 `lls_base_score` |
| `ccws_lls_update(wid)` — VTA hit → score increment | ✓ 在 `ccws_vta_probe_miss()` VTA hit 分支调用 |
| LLS decay in `ldst_unit::cycle()` | ✓ 每 `lls_decay_interval` 周期扫描所有 warp，decrement floor = `lls_base_score` |
| 6 新 config knob | ✓ `enable_lls_score`, `lls_base_score(100)`, `lls_hit_increment(1)`, `lls_decay_interval(100)`, `lls_decay_amount(1)`, `lls_max_score(1024)` |
| 8 新 stats 指标 | ✓ `score_update`, `decay_events`, `increment_total`, `decay_total`, `saturations`, `nonzero_warps`, `max_score`, `sum_score` |
| stats 聚合链 | ✓ `ldst_unit` → `shader_core_ctx` → `simt_core_cluster` → `print_stats()` |
| `SM7_QV100_ccws_lls_score_on/` config | ✓ 创建；`enable_ccws=1`, `vta_probe=1`, `lls_score=1` |
| quick set 7/7 pass | ✓ `sim_cycle` 不变，`lls_score_update = vta_hit`（完全相等），`load_gate_block=0` |
| `feature_off` 7/7 pass | ✓ 所有 LLS 计数为 0 |
| 编译 | ✓ warnings only（pre-existing Wreorder） |

### 关键验证结果

| Workload | vta_hit | lls_update | lls_update=vta_hit | gate_block |
|----------|---------|------------|-------------------|------------|
| vecadd | 72 | 72 | ✓ | 0 |
| strided_access | 24 | 24 | ✓ | 0 |
| page_stride_access | 24 | 24 | ✓ | 0 |
| atomic_contention | 0 | 0 | ✓ | 0 |
| mutual_tiled | 384 | 384 | ✓ | 0 |
| polybench_2dconv | 5616 | 5616 | ✓ | 0 |
| rodinia_hotspot | 1328 | 1328 | ✓ | 0 |

**关键发现**：`lls_score_update = vta_hit`（精确相等），证明 LLS update path 正确触发。`mutual_tiled` 末尾 `nonzero_warps=0`（decay 平衡了 hits，score 回到 base）。`polybench_2dconv` 末尾 80 个 warp 略高于 base（hits > decay 在该 workload 时长内）。

### 修改文件（本轮）

| 文件 | 修改内容 |
|------|---------|
| `src/gpgpu-sim/shader.h` | LLS state (6 vars + `ccws_lls_update` + `get_ccws_lls_stats`) in `ldst_unit`; 6 new config knobs in `shader_core_config`; declarations in `shader_core_ctx`, `simt_core_cluster` |
| `src/gpgpu-sim/shader.cc` | `init()`: LLS alloc; `ccws_vta_probe_miss()`: call `ccws_lls_update` on hit; `ldst_unit::cycle()`: decay; define 4 new methods |
| `src/gpgpu-sim/gpu-sim.cc` | Register 6 knobs; add LLS stats block in `print_stats()` |
| `configs/hrl-repro/SM7_QV100_ccws_lls_score_on/` | 新建 |
| `experiments/paper-ccws/lls_score_check.csv` | 新建（2×7 = 14 runs） |
| `experiments/paper-ccws/config_matrix.csv` | +1 行 |
| `docs/papers/ccws_round_w_lls_score.md` | 新建 |
| `docs/papers/ccws_repro_plan.md` | Stage S5 ✓, Round W note 加入 |
| `CLAUDE.md` | Round W 摘要 |

## Round Y: CCWS Minimal Real Load-Only Gating（2026-05-01）

### 本轮目标

将 Round X 的 would-gate telemetry 升级为真实 load-only gating。**最小实现，不改 cache replacement / VTA / LLS 逻辑，不重构 scheduler。**

### 完成内容

| 内容 | 结果 |
|------|------|
| `ccws_lg_gate_load(wid)` in `ldst_unit` | ✓ 查询 `would_can_issue[wid]`，返回 true = 阻塞 |
| `scheduler_unit::cycle()` gate 插入 | ✓ LOAD_OP / TENSOR_CORE_LOAD_OP 前加 `ccws_load_blocked` 判断 |
| 3 个 lg 计数器 | ✓ `m_ccws_lg_attempt/block/allow` in `ldst_unit` |
| 2 个新 config knob | ✓ `enable_load_gating(0)`, `load_gate_debug(0)` |
| stats 聚合链 | ✓ `ldst_unit → shader_core_ctx → simt_core_cluster → print_stats()` |
| `SM7_QV100_ccws_load_gate_on/` config | ✓ 基于 would_gate_on + `enable_load_gating=1` |
| feature_off quick set 7/7 | ✓ cycle=baseline，所有计数器=0 |
| load_gate_on quick set 7/7 | ✓ `rodinia_hotspot` `lg_block=5`（真实 gating 生效） |
| `lg_block = wg_block` | ✓ gate 与 telemetry 完全一致 |
| STORE / compute 不受影响 | ✓ 只 gate LOAD_OP / TENSOR_CORE_LOAD_OP |
| 编译 | ✓ warnings only（pre-existing Wreorder） |

### 关键验证结果（load_gate_on）

| Workload | sim_cycle | lg_attempt | lg_block | wg_block |
|----------|-----------|------------|----------|----------|
| vecadd | 5569 | 105 | 0 | 0 |
| strided_access | 5825 | 97 | 0 | 0 |
| page_stride_access | 5851 | 97 | 0 | 0 |
| atomic_contention | 5414 | 50 | 0 | 0 |
| mutual_tiled | 7479 | 2552 | 0 | 0 |
| polybench_2dconv | 6652 | 10232 | 0 | 0 |
| **rodinia_hotspot** | **6931** | **16420** | **5** | **5** |

**lg_block=5 in rodinia_hotspot**：真实 gating 生效 ✓。quick workload 太小，standard set 预期更多 block。

### 修改文件（本轮）

| 文件 | 修改内容 |
|------|---------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`: 3 lg counters + `ccws_lg_gate_load()` + `get_ccws_lg_stats()`; `shader_core_config`: 2 new knobs; `shader_core_ctx`/`simt_core_cluster`: declarations |
| `src/gpgpu-sim/shader.cc` | `init()`: lg counter init; `scheduler_unit::cycle()`: gate block; `ccws_lg_gate_load()`, `get_ccws_lg_stats()` definitions; aggregation methods |
| `src/gpgpu-sim/gpu-sim.cc` | Register 2 knobs; `paper_ccws_load_gate_*` from hardcoded 0 to real aggregation |
| `configs/hrl-repro/SM7_QV100_ccws_load_gate_on/` | 新建 |
| `experiments/paper-ccws/load_gating_check.csv` | 新建（2×7 = 14 runs） |
| `experiments/paper-ccws/config_matrix.csv` | +1 行 |
| `docs/papers/ccws_round_y_load_gating.md` | 新建 |
| `docs/papers/ccws_repro_plan.md` | Stage S6/S7 ✓, Round Y note 加入 |
| `CLAUDE.md` | Round Y 摘要 |

### 建议提交

```bash
git add src/gpgpu-sim/shader.h src/gpgpu-sim/shader.cc src/gpgpu-sim/gpu-sim.cc \
    configs/hrl-repro/SM7_QV100_ccws_load_gate_on/ \
    docs/papers/ccws_round_y_load_gating.md \
    experiments/paper-ccws/load_gating_check.csv \
    experiments/paper-ccws/config_matrix.csv \
    docs/papers/ccws_repro_plan.md CLAUDE.md
git commit -m "ccws: add minimal real load-only gating (Round Y)"
git tag ccws-load-gating-minimal
```


