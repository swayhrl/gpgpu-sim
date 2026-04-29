# Day1 阅读笔记：主循环 / Warp 调度 / Scoreboard

基于 GPGPU-Sim 4.2.0，commit `a4ce3fe`，配置 SM7_QV100 (Volta)。

---

## 1. 顶层调用链

```
gpgpusim_entrypoint.cc
  gpgpu_sim_thread_sequential()   // 或 gpgpu_sim_thread_concurrent()
    while (g_the_gpu->active())
      g_the_gpu->cycle()                      // gpgpu_sim::cycle(), gpu-sim.cc:1970
        m_cluster[i]->icnt_cycle()            // CORE 域：拉取 interconnect 返回的内存响应
        m_cluster[i]->core_cycle()            // CORE 域：推进所有 SM pipeline
          shader_core_ctx::cycle()            // shader.cc:3664
            writeback()
            execute()
            read_operands()
            issue()                           // shader.cc:1128
              schedulers[j]->cycle()          // scheduler_unit::cycle(), shader.cc:1259
                issue_warp()                  // shader.cc:1036
                  scoreboard.reserveRegisters()
```

外层循环条件 `active()` 在所有 CTA 完成且无更多 CTA 待分配时变为 false，循环结束后调用
`print_stats()` 和 `update_stats()`（含 `gpu_tot_sim_cycle += gpu_sim_cycle`）。

---

## 2. `gpgpu_sim::cycle()` 如何推进各时钟域

**文件**：`src/gpgpu-sim/gpu-sim.cc`，第 1970 行。

函数首先调用 `next_clock_domain()` 返回本次激活的时钟域掩码（`CORE / ICNT / L2 / DRAM`
以不同相对频率触发），然后按以下顺序执行：

| 步骤 | 掩码条件 | 操作 | 关键代码行 |
|------|----------|------|-----------|
| ① | `CORE` | `m_cluster[i]->icnt_cycle()` — 将 interconnect 中的内存响应弹入 shader cluster | 1973–1977 |
| ② | `ICNT` | `icnt_push()` — 把各 L2 sub-partition 顶部的响应推入 interconnect，设置 `IN_ICNT_TO_SHADER` 状态 | 1979–2001 |
| ③ | `DRAM` | `m_memory_partition_unit[i]->dram_cycle()` — 推进 DRAM 调度器与时序模型 | 2004–2023 |
| ④ | `L2` | `icnt_pop()` + `m_memory_sub_partition[i]->push()` + `cache_cycle()` — 从 interconnect 拉取来自核心的请求，推进 L2 cache | 2027–2047 |
| ⑤ | `ICNT` | `icnt_transfer()` — 推进 interconnect 内部路由仿真 | 2054–2056 |
| ⑥ | `CORE` | `m_cluster[i]->core_cycle()` — 推进所有 SM 的完整 pipeline | 2058–2077 |
| ⑦ | `CORE` | `gpu_sim_cycle++` | 2091 |

**关键设计**：CORE 域在一次 `cycle()` 调用中出现**两次**——先拉取内存响应（①），再推进
pipeline（⑥），确保当拍返回的数据在同一 cycle 内可被 pipeline 消费。
`gpu_sim_cycle` 仅在 CORE 域触发时递增，保证 cycle 计数与核心时钟对齐。

---

## 3. `shader_core_ctx::cycle()` 的 pipeline stage 顺序

**文件**：`src/gpgpu-sim/shader.cc`，第 3664 行。

```cpp
void shader_core_ctx::cycle() {
  if (!isactive() && get_not_completed() == 0) return;
  m_stats->shader_cycles[m_sid]++;
  writeback();       // EX_WB → 结果写回，释放 scoreboard
  execute();         // OC_EX → 功能单元执行
  read_operands();   // ID_OC → 操作数收集器读取寄存器文件
  issue();           // 调度器选 warp，写入 ID_OC pipeline register
  for (unsigned i = 0; i < m_config->inst_fetch_throughput; ++i) {
    decode();        // 从 fetch buffer 解码，写入 ibuffer
    fetch();         // 访问 L1I cache，填充 fetch buffer
  }
}
```

**逆流水线顺序**：从最靠近输出端的 stage 开始推进，目的是让每个 stage 先腾出空间再被
上游填充，避免同一周期内指令连跳两级。

`issue()` 内部（第 1128 行）以 round-robin 方式依次调用各 `scheduler_unit::cycle()`：

```cpp
void shader_core_ctx::issue() {
  for (unsigned i = 0; i < schedulers.size(); i++) {
    unsigned j = (Issue_Prio + i) % schedulers.size();
    schedulers[j]->cycle();
  }
  Issue_Prio = (Issue_Prio + 1) % schedulers.size();
}
```

SM7_QV100 配置 `gpgpu_num_sched_per_core = 4`，每个 SM 有 4 个 `scheduler_unit`，
`Issue_Prio` 每周期递增保证 4 个调度器之间公平轮转。

---

## 4. `scheduler_unit::cycle()` 如何选择 ready warp

**文件**：`src/gpgpu-sim/shader.cc`，第 1259 行。配置策略：`gpgpu_scheduler = lrr`。

**步骤：**

1. **`order_warps()`** — `lrr_scheduler` 调用 `order_lrr()`，从上次 issue 的 warp 之后
   轮转，生成 `m_next_cycle_prioritized_warps` 优先队列。

2. **遍历优先队列**，对每个 warp 的 ibuffer 头部指令依次检查（内层 while 循环，
   最多发射 `gpgpu_max_insn_issue_per_warp` 条）：

   | 检查项 | 失败处理 |
   |--------|----------|
   | `!warp.waiting() && !warp.ibuffer_empty()` | 跳过，继续下一 warp |
   | `pc == pI->pc`（无控制冒险） | 不等则 `ibuffer_flush()` + `set_next_pc(pc)` |
   | `!m_scoreboard->checkCollision(warp_id, pI)` | 失败则 stall，继续下一 warp |
   | 目标功能单元有空槽（`m_mem_out->has_free()` 等） | 失败则 stall |

3. 全部通过后调用 `m_shader->issue_warp(pipe_reg, pI, active_mask, warp_id, m_id)`。

---

## 5. Scoreboard：`checkCollision` / `reserveRegisters` / `releaseRegister`

**文件**：`src/gpgpu-sim/scoreboard.cc`。

**数据结构：**
- `reg_table[warp_id]`：`std::set<unsigned>` — 该 warp 所有待写入的目标寄存器编号
- `longopregs[warp_id]`：`std::set<unsigned>` — 依赖长延迟操作（global/local/tex load）的寄存器

**`checkCollision(wid, inst)`**（第 128 行）：
收集指令的全部 `out[]`、`in[]`、`pred`、`ar1`、`ar2`，任意一个存在于 `reg_table[wid]`
即返回 `true`（RAW 或 WAW hazard，触发 stall）。无 WAR 检测，因为 in-order issue。

**`reserveRegisters(inst)`**（第 83 行）：
在 `issue_warp()` 第 1124 行，issue 完成后**立即**将 `out[]` 插入 `reg_table[warp_id]`。
若为 global/local/tex load，同时插入 `longopregs`。

**`releaseRegister(wid, reg)`** 调用时机：

| 行号 | 调用位置 | 触发条件 |
|------|----------|----------|
| 1964 | `shader_core_ctx::writeback()` | ALU/SFU 等普通指令完成 writeback |
| 2149 | `ldst_unit::L1_latency_queue_cycle()` | load 命中 L1D cache，提前释放 |
| 2705/2710 | `ldst_unit::writeback()` | global/shared load 从内存系统返回 |
| 2955 | operand collector dispatch | LDGSTS 特殊路径 |

---

## 6. 一条 warp 指令从 ready 到 issue 再到 scoreboard 释放

```
ibuffer 头部指令有效
  → scheduler_unit::cycle() 四项检查全部通过
  → issue_warp()
      ├─ pipe_reg_set.get_free()        ← 占用 ID_OC_MEM/SP/INT 等 pipeline register
      ├─ warp.ibuffer_free()            ← 释放 ibuffer 槽，warp 可继续 fetch/decode
      ├─ **pipe_reg = *inst             ← 复制静态指令信息
      ├─ pipe_reg->issue(active_mask, warp_id, cycle, ...)  ← 填入动态信息
      ├─ func_exec_inst()               ← PTX 功能仿真（计算结果，不影响时序）
      └─ scoreboard.reserveRegisters()  ← out[] 进入 reg_table，后续同 warp 指令若依赖
                                           这些寄存器将在 checkCollision() 处 stall
  → read_operands()                     ← 操作数收集器读寄存器文件 → OC_EX pipeline register
  → execute() / ldst_unit::cycle()      ← MEM 指令发往 L1D cache
      ├─ L1D 命中 → ldst_unit::L1_latency_queue_cycle() → releaseRegister()  (早释放)
      └─ L1D 缺失 → push_to_icnt() → L2 → DRAM → 返回
  → ldst_unit::writeback() / shader_core_ctx::writeback()
      └─ releaseRegister() / releaseRegisters()  ← reg_table 清除，下一周期同 warp
                                                    后续指令可通过 checkCollision()
```

**vecadd baseline 观测**：`W0_Scoreboard = 1154` 周期（占总 5569 周期约 21%），
即 8 个 warp 中大量周期阻塞在等待 global load 返回（`averagemflatency = 211` 周期）。

---

## 7. 与 Day2 访存 / cache / MMU 阅读的连接点

| 连接点 | 文件 | 说明 |
|--------|------|------|
| `ldst_unit::cycle()` | `shader.cc` | MEM 指令出 `OC_EX_MEM`，进入 L1D cache 查找，Day2 起点 |
| `data_cache::access()` | `gpu-cache.h/cc` | L1D/L2 命中/缺失逻辑，缺失时生成 `mem_fetch` |
| `m_cluster->push_to_icnt()` | `shader.cc` | L1D miss 将请求推入 interconnect |
| `memory_sub_partition::cache_cycle()` | `l2cache.cc` | L2 处理来自 interconnect 的请求 |
| `memory_partition_unit::dram_cycle()` | `dram.cc` | L2 miss 转 DRAM 调度，响应原路返回 |
| `ldst_unit::writeback()` 中 `releaseRegister` | `shader.cc`:2705/2710 | 内存延迟的终点，warp 解除 scoreboard stall |

**Day2 建议**：从 `ldst_unit::cycle()` 出发，顺着
`L1D miss → push_to_icnt → cache_cycle → dram_cycle → icnt_pop → ldst_unit::writeback`
完整路径阅读，重点关注 `mem_fetch` 状态机（`IN_L1I_MISS_QUEUE` 等枚举值）和各级
cache 的替换策略入口（`gpu-cache.cc` 中 `tag_array::access()`）。
