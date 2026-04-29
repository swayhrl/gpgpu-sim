# GPGPU-Sim Development Notes

_Last updated: 2026-04-29 — Day4 TLB/VM hook reading completed; minimum TLB insertion plan ready._

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

- [ ] 进入最小 TLB latency model 实现（`shader.h` + `shader.cc` + `gpu-sim.cc` option registration）
- [ ] 用 vecadd baseline 验证：zero-latency TLB 结果应与当前 baseline 完全相同
- [ ] 开启 TLB miss latency 后，对比 `gpu_tot_sim_cycle` 和 `averagemflatency` 变化

## Known limitations

| Issue | Detail |
|-------|--------|
| No real GPU | `nvidia-smi` unavailable in container |
| PTX simulation only | No real SASS execution |
| No OpenCL | Driver not installed |
| Slow simulation | ~203268x slower than real hardware — keep kernels small |
| No cuDNN/cuBLAS | Not installed |
| Env not persistent | Re-source `load_gpgpusim.sh` in each new shell (CUDA path is in `~/.bashrc`) |
