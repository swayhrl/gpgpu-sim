# GPGPU-Sim Development Notes

_Last updated: 2026-04-30 — Round G complete; Rodinia 4 benchmarks (pathfinder/hotspot/srad_v2/lud) built and verified under GPGPU-Sim. Total: 14 workloads ready._

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
- [ ] **Round H（下一步选项）**：
  - A：继续补 Rodinia 剩余 app（nw / bfs / backprop）扩大覆盖类型
  - B：整理最终 manifest，为每个 workload 加 `min_recommended_size` 字段（如 lud 建议 `-s 64`）
  - C：实现最小 TLB latency model（`shader.h` + `shader.cc` + `gpu-sim.cc`）
- [ ] 用所有 workload 验证 zero-latency TLB（结果应与当前 baseline 完全一致）
- [ ] 开启 TLB miss latency = 100 cycles，对比 `page_stride_access` vs `polybench_gemm` cycle 差值

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
