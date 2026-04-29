● Day2 Coalescing / mem_fetch / L1D Miss Path Reading Notes

  基于 GPGPU-Sim 4.2.0，commit a4ce3fe，配置 SM7_QV100 (Volta)。

  ---
  1. Day1 到 Day2 的问题继承

  Day1 确认：W0_Scoreboard = 1154 / 5569 周期（约 21%）是 vecadd 主要瓶颈，根因是 warp 发射 global load 后，目标寄存器被 scoreboard 锁住，直到数据从内存返回才释放。averagemflatency = 211 cycles 说明每次
   global load 的端到端延迟远超 SM pipeline 深度。

  Day2 目标：追清 scoreboard 为什么必须等到 L1D miss 的整条链路返回才能 release，并找出 mf 离开 shader core 的准确出口，为 Day3 的 L2/DRAM 路径做铺垫。

  ---
  2. 从 warp_inst_t 到 mem_access_t 的调用链

  关键文件：src/abstract_hardware_model.cc:286

  调用时机：issue_warp() → func_exec_inst() 完成 PTX 功能仿真，每线程地址写入 m_per_scalar_thread[thread].memreqaddr[]，随即调用 warp_inst_t::generate_mem_accesses()。

  memory_coalescing_arch() 核心逻辑（coalesce_arch=70，即 Volta）：
  - sector_segment_size = true，对 4B 数据取 segment_size = 32
  - 对每个 subwarp 遍历活跃线程，按 32B 边界归组到 subwarp_transactions map
  - 对每个 segment 调用 memory_coalescing_arch_reduce_and_send()，按 chunk 位图压缩后 m_accessq.push_back(mem_access_t(...))

  mem_access_t 包含：access type（GLOBAL_ACC_R / GLOBAL_ACC_W）、sector-aligned 地址、size（32B 或更小）、active mask、byte mask、sector mask。

  vecadd 分析：32 线程 × 4B float，覆盖 128B = 4 × 32B sector，每个 warp 产生 4 个 mem_access_t。极端情况：stride=32B 时每线程一个 sector → 最多 32 个；完全重叠时 → 1 个。

  ---
  3. 从 mem_access_t 到 mem_fetch 的调用链

  关键文件：src/gpgpu-sim/shader.cc:2062（process_memory_access_queue_l1cache）、shader.cc:68（shader_core_mem_fetch_allocator::alloc）

  ldst_unit::issue() 先记录 m_pending_writes[warp_id][reg_id] += n_accesses（n_accesses = accessq_count()），然后调用 pipelined_simd_unit::issue()，指令进入 dispatch register。

  之后在 ldst_unit::cycle() → memory_cycle() → process_memory_access_queue_l1cache() 中：
  - 每 cycle 最多处理 l1_banks=4 个 mem_access_t
  - 对每个调用 m_mf_allocator->alloc(inst, accessq_back(), cycle) 创建 mem_fetch

  mem_fetch 相比 mem_access_t 增加：

  ┌───────────────────────────────┬───────────────────────────────────────────────────────┐
  │           新增字段            │                         用途                          │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_sid / m_tpc / m_wid         │ 路由返回到正确 SM/cluster/warp                        │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_status / m_status_change    │ 状态机（IN_L1D_MISS_QUEUE 等 27 个状态）              │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_partition_addr / m_raw_addr │ 解码后的 DRAM chip-row-bank-col，用于 L2/DRAM 路由    │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_type                        │ READ_REQUEST / WRITE_REQUEST / READ_REPLY / WRITE_ACK │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_timestamp / m_timestamp2    │ 用于统计 averagemflatency                             │
  ├───────────────────────────────┼───────────────────────────────────────────────────────┤
  │ m_ctrl_size                   │ 控制包大小，决定 interconnect 占用的 flit 数          │
  └───────────────────────────────┴───────────────────────────────────────────────────────┘

  ---
  4. ldst_unit 到 L1D 的调用链

  关键文件：shader.cc:2834（ldst_unit::cycle）、shader.cc:2121（L1_latency_queue_cycle）

  SM7_QV100 配置：gmem_skip_L1D=0（global memory 不跳过 L1D），l1_latency=20，l1_banks=4。

  ldst_unit::cycle() 每周期执行顺序：
  1. writeback() — 处理已返回的 mf，释放 scoreboard
  2. 消费 m_response_fifo（来自下级内存的返回）→ m_L1D->fill() 或 m_next_global
  3. m_L1D->cycle() — L1D 内部 cycle
  4. L1_latency_queue_cycle() — 20 级 shift register，模拟 tag lookup pipeline
  5. memory_cycle() → process_memory_access_queue_l1cache() — 把新 mf 入队尾

  l1_latency_queue[bank_id][0..19]：mf 进入第 19 级（队尾），每 cycle 向前移动一格，到达第 0 级（队头）时，才真正调用 m_L1D->access(addr, mf, time, events)。这 20 cycle 模拟 L1D 的 pipeline 延迟（tag
  lookup + bank arbitration）。

  ---
  5. L1D HIT / MISS / MSHR_HIT / RESERVATION_FAIL 分支

  关键文件：gpu-cache.cc:1977（data_cache::access）、gpu-cache.cc:1354（send_read_request）

  调用链：data_cache::access() → tag_array::probe() → process_tag_probe() → m_rd_miss() → send_read_request()

  tag_array::probe() 返回值含义：
  - HIT：tag 匹配且 sector status == VALID / MODIFIED（可读）
  - HIT_RESERVED：tag 匹配但 sector status == RESERVED（已分配但 fill 未到）
  - SECTOR_MISS：tag 匹配，但所需 sector 无效
  - RESERVATION_FAIL：set 内所有 way 均为 RESERVED（all_reserved=true），无法分配新行

  send_read_request() 逻辑（gpu-cache.cc:1354）：

  ┌──────────────────────────┬─────────────────┬─────────────────────────────────────────────────────┐
  │        MSHR 状态         │ miss queue 状态 │                        结果                         │
  ├──────────────────────────┼─────────────────┼─────────────────────────────────────────────────────┤
  │ mshr_hit && mshr_avail   │ —               │ MSHR_HIT：合并入已有 MSHR entry，无需新发 miss 请求 │
  ├──────────────────────────┼─────────────────┼─────────────────────────────────────────────────────┤
  │ !mshr_hit && mshr_avail  │ 未满            │ MISS：新建 MSHR entry，m_miss_queue.push_back(mf)   │
  ├──────────────────────────┼─────────────────┼─────────────────────────────────────────────────────┤
  │ mshr_hit && !mshr_avail  │ —               │ RESERVATION_FAIL（MSHR_MERGE_ENTRY_FAIL）           │
  ├──────────────────────────┼─────────────────┼─────────────────────────────────────────────────────┤
  │ !mshr_hit && !mshr_avail │ —               │ RESERVATION_FAIL（MSHR_ENTRY_FAIL）                 │
  ├──────────────────────────┼─────────────────┼─────────────────────────────────────────────────────┤
  │ 任意                     │ 满              │ RESERVATION_FAIL                                    │
  └──────────────────────────┴─────────────────┴─────────────────────────────────────────────────────┘

  ---
  6. L1D hit 与 releaseRegister 的关系

  关键文件：shader.cc:2121（L1_latency_queue_cycle）

  L1_latency_queue_cycle() 在 status == HIT 时：
  l1_latency_queue[j][0] = NULL;
  --m_pending_writes[wid][out[r]];
  if (still_pending == 0)
      m_scoreboard->releaseRegister(wid, out[r]);

  关键设计：ldst_unit::issue() 时将 m_pending_writes 初始化为 n_accesses（该指令产生的 mem_access_t 数量，如 4）。每个 access 返回 HIT 时各自递减一次，全部到 0 才 release。这防止同一指令的多个 sector
  请求只返回部分时就错误释放寄存器。

  L1D hit 可以提前释放的原因：20-cycle 延迟队列走到头后，tag 命中 → 数据已在 cache 中，不需要等 interconnect/L2/DRAM。从 issue 到 release 约 20+ cycle，远短于 miss 路径的 211 cycle。

  ---
  7. L1D miss 如何进入 m_miss_queue 并离开 shader core

  关键文件：gpu-cache.cc:1215（baseline_cache::cycle）、shader.h:2734（shader_memory_interface）

  1. send_read_request() → m_miss_queue.push_back(mf) + mf->set_status(IN_L1D_MISS_QUEUE, time)
  2. 每 cycle baseline_cache::cycle() 检查 m_miss_queue 队头：若 !m_memport->full(mf->size(), ...) 则 pop + m_memport->push(mf)
  3. m_memport 是 shader_memory_interface，其 push() 调用 m_cluster->icnt_inject_request_packet(mf)
  4. icnt_inject_request_packet() 设置 mf->m_status = IN_ICNT_TO_MEM，然后调用 ::icnt_push(cluster_id, dest_partition, mf, packet_size)

  至此 mf 正式离开 shader core，进入 interconnect。dest_partition = mf->get_sub_partition_id()，由构造 mem_fetch 时的地址解码（m_raw_addr）决定，路由到目标 L2 sub-partition。

  MISS 和 HIT_RESERVED 时，L1_latency_queue_cycle() 将 l1_latency_queue[j][0] = NULL（pop 掉），但 不调用 releaseRegister。scoreboard 锁住的寄存器要等 mf 沿 interconnect → L2 → DRAM → 返回路径走完，才在
   ldst_unit::writeback() 中 release。

  ---
  8. baseline 数值如何映射到 Day2 机制

  ┌───────────────────────────────┬──────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │             指标              │ 数值 │                                                                    机制对应                                                                     │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ L1D core[0] access = 96       │ 96   │ 8 warps × 4 accesses/warp × (load_a + load_b + store_c) = 8×4×3 = 96 次 m_L1D->access() 调用                                                    │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ L1D miss = 96                 │ 96   │ 冷启动，tag array 全空，所有 sector 都是首次访问 → MISS                                                                                         │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ L1D reservation fail = 79     │ 79   │ MSHR 同时持有大量 RESERVED 行时，后续请求在 tag_array::probe() 处找不到可用 way，all_reserved=true → 退回 l1_latency_queue[j][0]，下 cycle 重试 │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ L2 accesses = 96              │ 96   │ L1D 所有 96 次 MISS 均通过 m_miss_queue → interconnect → L2，与 L1D miss 数一致                                                                 │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ averagemflatency = 211 cycles │ 211  │ 从 mem_fetch 创建（m_timestamp）到 fill 返回（m_timestamp2）的平均值，包含 icnt 排队 + L2 access + DRAM（compulsory miss 部分） + 返回 icnt     │
  ├───────────────────────────────┼──────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ W0_Scoreboard = 1154 cycles   │ 1154 │ mf 在 interconnect+L2+DRAM 路径上停留 ~211 cycle，期间同 warp 后续指令若依赖 load 目标寄存器，全部在 checkCollision() 处 stall                  │
  └───────────────────────────────┴──────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---
  9. 后续 cache/MMU 改动可能插入的位置

  ┌───────────────────────┬─────────────────────────────────────────────────────────────────────┬───────────────────────────────────────────────────────────────┐
  │       插入位置        │                               文件:行                               │                           改动类型                            │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ 地址翻译（VA→PA）     │ shader.cc:2073，process_memory_access_queue_l1cache 创建 mf 之后    │ 插入 TLB 查找，MISS 时 stall 或进入 IN_L1TLB_MISS_QUEUE       │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ L1D 替换策略          │ gpu-cache.cc:246，tag_array::probe 的 valid_line 选择段             │ 修改 LRU/FIFO/自定义策略                                      │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ MSHR 合并逻辑         │ gpu-cache.cc:1354，send_read_request                                │ 修改 MSHR 条目数或 merge 上限（m_num_entries / m_max_merged） │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ miss queue 大小       │ gpu-cache.cc:1375，m_miss_queue.size() < m_config.m_miss_queue_size │ 调整 miss queue 容量影响 RESERVATION_FAIL 频率                │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ L1D→interconnect 带宽 │ gpu-cache.h:1215，baseline_cache::cycle() 中 m_memport->full() 判断 │ 限速逻辑                                                      │
  ├───────────────────────┼─────────────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────────────────┤
  │ prefetch              │ ldst_unit::cycle() 中 memory_cycle() 之后                           │ 提前生成额外 mem_fetch 推入 m_miss_queue                      │
  └───────────────────────┴─────────────────────────────────────────────────────────────────────┴───────────────────────────────────────────────────────────────┘

  ---
  10. Day3：L2 / interconnect / DRAM 阅读建议

  Day3 起点（精确定位）：

  baseline_cache::cycle()           // gpu-cache.cc:1215
    → m_memport->push(mf)           // mf.status = IN_ICNT_TO_MEM
    → simt_core_cluster::icnt_inject_request_packet()  // shader.cc:4606
    → ::icnt_push(cluster_id, dest, mf, size)          // 进入 interconnect

  Day3 建议阅读顺序：

  1. interconnect 接收侧：gpgpu_sim::cycle() 步骤 ④ → icnt_pop() → memory_sub_partition::push() → mf status = IN_PARTITION_ICNT_TO_L2_QUEUE
  2. L2 access：memory_sub_partition::cache_cycle() → l2_cache::access() → 复用同一套 data_cache::access() 框架；注意 L2 miss 时进入 IN_PARTITION_L2_TO_DRAM_QUEUE
  3. DRAM 调度：memory_partition_unit::dram_cycle() → dram_t::cycle() → FR-FCFS 调度器，注意 IN_PARTITION_DRAM 状态
  4. 返回路径：DRAM 完成 → IN_PARTITION_DRAM_TO_L2_QUEUE → L2 fill → IN_PARTITION_L2_TO_ICNT_QUEUE → icnt_push() 返回 → gpgpu_sim::cycle() 步骤 ① m_cluster->icnt_cycle() → ldst_unit::fill() →
  m_response_fifo → writeback() → releaseRegister()
  5. 重点关注：mem_fetch_status.tup 中 IN_PARTITION_* 系列状态的转换顺序；averagemflatency 如何由 m_timestamp 和 m_timestamp2 计算得出
