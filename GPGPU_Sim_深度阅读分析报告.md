# GPGPU-Sim 深度阅读分析报告

## 一、当前仓库判断

工作区根目录：`d:\repo`，包含两个独立仓库：

- `gpgpu-sim_distribution/` — GPGPU-Sim 主模拟器，本轮深读对象
- `accel-sim-framework/` — Accel-Sim，本轮只记录路径，不展开

GPGPU-Sim 版本特征：包含 `SM86_RTX3070`、`SM75_RTX2060`、`SM7_QV100` 等 tested-cfgs，支持 Volta/Turing/Ampere 配置；含 `accelwattch/` 功耗模型（非旧版 GPUWattch）；含 `intersim2/`（BookSim2 互连）；含 tensor core 支持。这是一个较新的 GPGPU-Sim 版本（4.x 系列），与早期 3.x 版本相比增加了 sector cache、sub-partition、tensor core、LDGSTS 等特性。

---

## 二、GPGPU-Sim 顶层结构地图

```text
gpgpu-sim_distribution/
├── src/
│   ├── abstract_hardware_model.h/.cc  ← 核心抽象：warp_inst_t、mem_access_t、inst_t
│   ├── gpgpusim_entrypoint.h/.cc      ← 模拟器入口，CUDA API 拦截
│   ├── stream_manager.h/.cc           ← CUDA stream 管理，kernel launch
│   ├── option_parser.h/.cc            ← 配置文件解析
│   │
│   ├── gpgpu-sim/                     ← 性能模拟器核心
│   │   ├── gpu-sim.h/.cc              ← 顶层 gpgpu_sim 类、主 cycle() 循环
│   │   ├── shader.h/.cc               ← SM/shader core/warp/ldst_unit（最大文件）
│   │   ├── gpu-cache.h/.cc            ← 所有 cache 类（L1/L2/MSHR/tag_array）
│   │   ├── l2cache.h/.cc              ← memory_partition_unit / memory_sub_partition
│   │   ├── dram.h/.cc                 ← DRAM 控制器
│   │   ├── dram_sched.h/.cc           ← DRAM 调度器（FR-FCFS 等）
│   │   ├── mem_fetch.h/.cc            ← mem_fetch 请求对象
│   │   ├── addrdec.h/.cc              ← 地址解码（线性→chip/bank/row/col）
│   │   ├── icnt_wrapper.h/.cc         ← 互连接口包装（函数指针）
│   │   ├── local_interconnect.h/.cc   ← 简单 crossbar 互连
│   │   ├── scoreboard.h/.cc           ← 寄存器依赖跟踪
│   │   ├── mem_latency_stat.h/.cc     ← 内存延迟统计
│   │   └── stat-tool.h/.cc            ← 通用统计工具
│   │
│   ├── cuda-sim/                      ← PTX 功能模拟器
│   │   ├── cuda-sim.h/.cc             ← PTX 执行引擎
│   │   ├── ptx_ir.h/.cc               ← PTX IR 定义
│   │   ├── instructions.cc            ← 所有 PTX 指令语义（最大文件，199KB）
│   │   ├── memory.h/.cc               ← 功能模拟内存空间
│   │   └── ptx_sim.h/.cc              ← PTX 线程状态
│   │
│   ├── intersim2/                     ← BookSim2 网络模拟器
│   │   ├── interconnect_interface.hpp/.cpp ← GPGPU-Sim 与 BookSim2 的接口
│   │   └── gputrafficmanager.hpp/.cpp      ← GPU 流量管理器
│   │
│   └── accelwattch/                   ← 功耗模型（McPAT 派生）
│
├── libcuda/                           ← CUDA 运行时 API 拦截层
│   └── cuda_runtime_api.cc            ← cudaMalloc/cudaMemcpy/cudaLaunch 等
│
└── configs/tested-cfgs/               ← GPU 配置文件
    ├── SM7_QV100/gpgpusim.config      ← Volta GV100
    ├── SM75_RTX2060/gpgpusim.config   ← Turing
    └── SM86_RTX3070/gpgpusim.config   ← Ampere
```

按研究方向的文件归属：

| 方向 | 主要文件 |
|---|---|
| Shader core / SM / warp / issue | `shader.h`, `shader.cc` |
| Memory access path | `shader.cc` (ldst_unit), `abstract_hardware_model.cc` (coalescing) |
| L1D / L1T / L2 cache | `gpu-cache.h`, `gpu-cache.cc` |
| Memory partition / DRAM / interconnect | `l2cache.h`, `l2cache.cc`, `dram.h`, `dram.cc`, `icnt_wrapper.h` |
| PTX / functional sim / kernel launch | `cuda-sim.cc`, `instructions.cc`, `gpgpusim_entrypoint.cc` |
| 配置文件读取 | `gpu-sim.h` (`memory_config`, `shader_core_config`), `option_parser.cc` |
| 统计输出 | `gpu-sim.cc` (`gpu_print_stat`), `mem_latency_stat.h`, `stat-tool.h` |
| Accel-Sim 对照接口 | `abstract_hardware_model.h`, `mem_fetch.h`, `icnt_wrapper.h` |

---

## 三、GPGPU-Sim 访存路径主线

完整调用链（global memory load）

```text
PTX 功能模拟 (cuda-sim/instructions.cc)
└── 每线程计算地址 → warp_inst_t::set_addr()
    [abstract_hardware_model.h:1061]

warp_inst_t::generate_mem_accesses()
[abstract_hardware_model.cc:286]
└── memory_coalescing_arch()
    [abstract_hardware_model.cc:477]
    ├── 按 subwarp 分组，计算 segment 对齐
    └── memory_coalescing_arch_reduce_and_send()
        [abstract_hardware_model.cc:696]
        └── m_accessq.push_back(mem_access_t)
            ← 生成合并后的访问队列

ldst_unit::issue() [shader.cc]
├── 从 m_accessq 取出 mem_access_t
└── 分配 mem_fetch (shader_core_mem_fetch_allocator::alloc)
    [shader.cc:58]

ldst_unit::memory_cycle() [shader.cc:2215]
├── [bypass L1D: CACHE_GLOBAL / gmem_skip_L1D]
│   └── m_icnt->push(mf) → 直接进 interconnect
└── [走 L1D]
    └── process_memory_access_queue_l1cache()
        └── 放入 l1_latency_queue（模拟 L1 访问延迟）

ldst_unit::L1_latency_queue_cycle() [shader.cc:2121]
├── m_L1D->access(addr, mf, time, events)
│   [gpu-cache.cc → cache::access]
│   └── data_cache::access() [gpu-cache.cc:1977]
│       ├── tag_array::probe()
│       ├── process_tag_probe()
│       │   ├── HIT → 直接返回，释放 scoreboard
│       │   ├── MSHR_HIT → 合并到已有 MSHR 条目
│       │   └── MISS
│       │       └── send_read_request() [gpu-cache.cc:1341]
│       │           ├── mshr_table::add()
│       │           └── m_miss_queue.push_back(mf)
│       │               ← 进入 miss queue

baseline_cache::cycle() [gpu-cache.cc]
├── 从 m_miss_queue 取出 mf
└── m_memport->push(mf)
    ← 通过 mem_fetch_interface 发往 interconnect

==== interconnect (icnt_wrapper / intersim2) ====

memory_sub_partition::push() [l2cache.cc:786]
├── 接收来自 icnt 的 mf
└── 放入 m_icnt_L2_queue

memory_sub_partition::cache_cycle() [l2cache.cc:465]
├── 从 m_icnt_L2_queue 取出 mf
└── m_L2cache->access() [l2_cache::access]
    ├── L2 HIT → 放入 m_L2_icnt_queue（返回路径）
    └── L2 MISS
        └── 放入 m_L2_dram_queue

memory_partition_unit::dram_cycle() [l2cache.cc:306]
├── 从 m_L2_dram_queue 取出 mf
└── m_dram->push(mf) [dram.cc]
    ├── DRAM 调度器（FR-FCFS）处理
    └── dram_t::return_queue_push()

memory_partition_unit::dram_cycle()
├── 从 dram_return_queue 取出完成的 mf
├── mf->set_reply()（READ_REQUEST → READ_REPLY）
└── 放入 m_sub_partition[i]->m_dram_L2_queue

memory_sub_partition::cache_cycle()
├── L2 fill (tag_array::fill)
└── 放入 m_L2_icnt_queue

==== interconnect 返回路径 ====

simt_core_cluster::icnt_cycle() [shader.cc / gpu-sim.cc]
├── icnt_pop() → 取出 READ_REPLY
└── 路由到对应 shader core

shader_core_ctx::accept_fetch()
└── ldst_unit::fill(mf)
    └── 放入 m_response_fifo

ldst_unit::writeback()
├── 从 m_response_fifo 取出 mf
├── mshr_table::mark_ready()
├── 释放 scoreboard (pending writes--)
│   [scoreboard.cc::releaseRegister()]
└── warp 解除 stall，可重新调度
```

核心对象定义位置：

| 对象 | 文件 | 行号 |
|---|---|---|
| `new_addr_type` | `abstract_hardware_model.h` | 103 |
| `mem_access_t` | `abstract_hardware_model.h` | 809 |
| `warp_inst_t` | `abstract_hardware_model.h` | 1061 |
| `mem_fetch` | `mem_fetch.h` | 54 |
| `mshr_table` | `gpu-cache.h` | 1024 |
| `baseline_cache` | `gpu-cache.h` | 1280 |
| `data_cache` | `gpu-cache.h` | 1509 |
| `l1_cache` | `gpu-cache.h` | 1702 |
| `l2_cache` | `gpu-cache.h` | 1730 |
| `memory_partition_unit` | `l2cache.h` | 74 |
| `memory_sub_partition` | `l2cache.h` | 162 |
| `dram_t` | `dram.h` | 112 |
| `shader_core_ctx` | `shader.h` | 2059 |
| `ldst_unit` | `shader.h` | ~1345 |
| `scoreboard` | `scoreboard.h` | — |

---

## 四、cache/MMU/访存改动入口索引

### 修改 L2 replacement policy

- `gpu-cache.h` 中 `tag_array` 类、`probe()` 和 `access()` 函数
- `gpu-cache.cc` 中 `tag_array::access()` — 替换策略在此选择 victim line
- 配置参数：`gpgpu_cache:dl2` 字符串中的替换策略字段（`L` = LRU, `F` = FIFO 等）
- `gpu-sim.h` 中 `cache_config` 类的 `m_replacement_policy` 字段

### 修改 L2 write allocate / writeback / sector behavior

- `gpu-cache.h` 中 `data_cache::init()` — 通过函数指针 `m_wr_hit` / `m_wr_miss` 选择策略
- `gpu-cache.cc` 中 `wr_hit_wb()`、`wr_miss_wa_naive()`、`wr_miss_wa_fetch_on_write()` 等函数
- Sector cache 行为：`tag_array::fill()` 中的 `SECTOR_MISS` 分支；`sector_cache_block::allocate_sector()`
- 配置：`gpgpu_cache:dl2` 字符串中的 write policy 字段

### 增加 cache bypass / cache hint / special memory region

- `shader.cc` 中 `ldst_unit::memory_cycle()` — bypass 判断逻辑（`CACHE_GLOBAL` 类型检查）
- `abstract_hardware_model.h` 中 `mem_access_type` 枚举 — 增加新的访问类型
- `gpu-cache.cc` 中 `data_cache::process_tag_probe()` — 在此加 bypass 逻辑

### 修改 MSHR / miss queue / writeback queue / fill path

- `gpu-cache.h` 中 `mshr_table` 类（行 1024）
- `gpu-cache.cc` 中 `baseline_cache::send_read_request()`、`baseline_cache::cycle()`、`baseline_cache::fill()`
- Miss queue：`baseline_cache::m_miss_queue` (`std::list<mem_fetch*>`)
- Fill path: `tag_array::fill()` → `baseline_cache::fill()` → `mshr_table::mark_ready()`

### 增加 TLB/MMU/page walk

- 目前 GPGPU-Sim 无 TLB/MMU 模块（搜索无 `*tlb*`/`*mmu*` 文件）
- 入手点：`shader.cc` 中 `ldst_unit::memory_cycle()` — 在 L1 访问前插入地址翻译
- 或在 `abstract_hardware_model.cc` 的 `generate_mem_accesses()` 后、`mem_fetch` 创建前插入 VA→PA 翻译
- `mem_fetch` 中已有 `m_raw_addr`（物理地址结构），可扩展为支持虚拟地址字段

### 修改 memory partition 和 DRAM 调度

- `l2cache.cc` 中 `memory_partition_unit::dram_cycle()` — 仲裁逻辑
- `dram_sched.h`/`dram_sched.cc` — FR-FCFS 调度器
- `dram.cc` 中 `dram_t::cycle()` — DRAM 时序状态机
- 配置：`gpgpu_dram_timing_opt` 字符串，`gpgpu_frfcfs_dram_sched_queue_size`

### 修改 interconnect / memory partition 之间的队列和流控

- `l2cache.h` 中 `memory_sub_partition` 的队列成员：`m_icnt_L2_queue`、`m_L2_icnt_queue`、`m_L2_dram_queue`、`m_dram_L2_queue`
- `l2cache.cc` 中 `memory_sub_partition::push()` — icnt→L2 入口
- `icnt_wrapper.cc` 中 `icnt_push`/`icnt_pop` 函数指针
- `intersim2/interconnect_interface.cpp` — BookSim2 接口

### 增加新的统计指标

- `gpu-sim.cc` 中 `gpu_print_stat()` — 在此添加 printf 输出
- `mem_latency_stat.h` 中 `memory_stats_t` — 添加新的统计成员
- `gpu-cache.h` 中 `cache_stats` 类 — cache 专项统计
- `stat-tool.h` — 通用统计工具（直方图等）

### 修改配置文件参数

- `gpu-sim.h` 中 `memory_config` 类 — 内存相关参数注册
- `shader.h` 中 `shader_core_config` 类 — core 相关参数注册
- 参数通过 `option_parser` 注册，格式：`option_parser_register(opp, "-param_name", ...)`
- 配置文件：`configs/tested-cfgs/SM7_QV100/gpgpusim.config`

---

## 五、GPGPU-Sim 优先阅读顺序（3～5 天计划）

### Day 1：主循环 + warp 调度 + 指令 issue

要看的文件：

- `gpu-sim.cc` — `gpgpu_sim::cycle()` 函数（约行 1970）
- `shader.h` — `shader_core_ctx`（行 2059）, `simt_core_cluster`（行 2612）
- `shader.cc` — `shader_core_ctx::cycle()`、`scheduler_unit::cycle()`

核心问题：

- `gpgpu_sim::cycle()` 如何驱动多个时钟域（core/icnt/L2/DRAM）？
- warp 调度器（GTO/LRR）如何选择下一条指令？
- scoreboard 如何阻止有 RAW 依赖的 warp issue？

建议打断点/加日志：

- `gpgpu_sim::cycle()` 入口
- `scheduler_unit::cycle()` 中 warp 选择逻辑
- `scoreboard::checkCollision()` 返回 true 的情况

读完后应能回答：

- 一个 cycle 内 core/icnt/DRAM 各执行几次？
- warp stall 的主要原因有哪些？

---

### Day 2：coalescing + mem_fetch 生成 + L1D 访问

要看的文件：

- `abstract_hardware_model.cc` — `generate_mem_accesses()`（行 286）、`memory_coalescing_arch()`（行 477）
- `shader.cc` — `ldst_unit::memory_cycle()`（行 2215）、`L1_latency_queue_cycle()`（行 2121）
- `gpu-cache.cc` — `data_cache::access()`（行 1977）、`send_read_request()`（行 1341）

核心问题：

- 32 个线程的 global load 如何合并成 1~N 个 `mem_access_t`？
- segment size 在不同架构（Fermi/Volta）下有何不同？
- L1D hit 和 MSHR_HIT 的处理路径有何区别？

建议打断点/加日志：

- `memory_coalescing_arch_reduce_and_send()` — 打印合并前后的地址数量
- `data_cache::access()` — 打印 `probe_status` 和 `access_status`
- `mshr_table::add()` — 打印 MSHR 合并情况

读完后应能回答：

- 一条 warp load 指令最多/最少产生几个 mem_fetch？
- MSHR 满时 warp 如何处理（RESERVATION_FAIL）？

---

### Day 3：L1 miss → interconnect → L2 → DRAM

要看的文件：

- `gpu-cache.cc` — `baseline_cache::cycle()`（miss queue 出队）、`baseline_cache::fill()`
- `l2cache.cc` — `memory_sub_partition::push()`（行 786）、`cache_cycle()`（行 465）
- `l2cache.cc` — `memory_partition_unit::dram_cycle()`（行 306）
- `icnt_wrapper.cc` — `icnt_push`/`icnt_pop` 接口

核心问题：

- miss queue 中的 mf 如何通过 `m_memport->push()` 进入 interconnect？
- `memory_sub_partition` 的四个队列（`icnt_L2`、`L2_icnt`、`L2_dram`、`dram_L2`）各自作用？
- L2 miss 后 mf 如何进入 DRAM 调度队列？

建议打断点/加日志：

- `memory_sub_partition::push()` — 打印 mf 的 addr 和 sub_partition_id
- `memory_sub_partition::cache_cycle()` — 打印 L2 hit/miss 情况
- `dram_t::push()` — 打印进入 DRAM 的请求

读完后应能回答：

- 从 L1 miss 到 DRAM 请求发出，经过哪些队列？各队列的深度限制是什么？

---

### Day 4：DRAM 返回 + response 路径 + warp 解除 stall

要看的文件：

- `dram.cc` — `dram_t::cycle()`、`return_queue_push()`
- `dram_sched.cc` — FR-FCFS 调度逻辑
- `l2cache.cc` — `dram_cycle` 中的返回处理
- `shader.cc` — `ldst_unit::fill()`、`ldst_unit::writeback()`
- `scoreboard.cc` — `releaseRegister()`

核心问题：

- DRAM 完成后 mf 如何经 L2 fill → icnt → shader core 返回？
- `ldst_unit::fill()` 和 `writeback()` 如何触发 scoreboard 释放？
- warp 从 stall 到重新可调度的完整状态转换？

建议打断点/加日志：

- `ldst_unit::fill()` — 打印 mf 的 wid 和 addr
- `scoreboard::releaseRegister()` — 打印释放的寄存器
- `scheduler_unit::cycle()` 中 warp 从 not-ready 变为 ready 的时刻

读完后应能回答：

- 一次 DRAM 访问的端到端延迟（cycle 数）大约是多少？
- 多个 warp 的
```
