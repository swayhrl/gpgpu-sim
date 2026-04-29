# Day3 L2 / Interconnect / DRAM / Return Path Reading Notes

---

## 1. Day2 到 Day3 的问题继承：从 L1D miss 到 averagemflatency

Day2 的出口：一个 L1D cache miss 在 `baseline_cache::access()` 中被注册到 MSHR，`mf` 被放入 `m_miss_queue`，status 变为 `IN_L1D_MISS_QUEUE`。

Day3 的中心问题：这个 `mf` 后续会经过哪些路径，以及 `averagemflatency = 211 cycles` 这个数值是如何逐步累积出来的？

`averagemflatency` 的定义（`mem_latency_stat.cc:296`）：

```text
mf_total_lat / num_mfs
mf_total_lat 在 memlatstat_read_done(mf) 中加和
=（当前 cycle）- mf->get_timestamp()
```

`m_timestamp` 是 `mem_fetch` constructor 的参数 cycle（= `gpu_sim_cycle + gpu_tot_sim_cycle`），实质上就是“mf 被生成的 cycle”作为起点。终点是 `simt_core_cluster::icnt_cycle()` 调用 `accept_ldst_unit_response(mf)` 之前（`shader.cc:4726`）。也就是说，“shader core cluster 接收到 mf 的 cycle”就是终点。

---

## 2. 总体调用链图

```text
gpu_sim cycle N (CORE clock)
└── simt_core_cluster::icnt_cycle()
    ├── m_response_fifo → accept_ldst_unit_response()
    └── icnt_pop(cluster_id) → m_response_fifo.push_back [IN_CLUSTER_TO_SHADER_QUEUE]

(ICNT clock)
└── for each sub_partition:
    m_memory_sub_partition[i]->top()
    icnt_push(mem2device(i), tpc, mf) [IN_ICNT_TO_SHADER]
    pop()

(DRAM clock)
└── memory_partition_unit::dram_cycle()
    ├── m_dram->return_queue_top() → dram_L2_queue_push [IN_PARTITION_DRAM_TO_L2_QUEUE]
    ├── m_dram->cycle()          → scheduler + timing + rwq
    └── m_dram_latency_queue → m_dram->push()           [IN_PARTITION_MC_INTERFACE_QUEUE]

(L2 clock)
└── for each sub_partition:
    icnt_pop(mem2device(i)) → sub_partition::push()     [IN_PARTITION_ICNT_TO_L2_QUEUE]
    sub_partition::cache_cycle()
    ├── access_ready() → m_L2_icnt_queue                [IN_PARTITION_L2_TO_ICNT_QUEUE]
    ├── dram_L2_queue → L2cache->fill()                 [IN_PARTITION_L2_FILL_QUEUE]
    ├── icnt_L2_queue → L2cache->access()
    │   ├── HIT  → m_L2_icnt_queue                      [IN_PARTITION_L2_TO_ICNT_QUEUE]
    │   └── MISS → L2interface::push → m_L2_dram_queue  [IN_PARTITION_L2_TO_DRAM_QUEUE]
    └── icnt_transfer()

(CORE clock)
└── core_cycle() → ldst_unit::cycle()
    ├── writeback() → releaseRegister()
    └── m_response_fifo → L1D::fill or m_next_global
```

---

## 3. Day2 出口：从 m_miss_queue 到 icnt_push

`baseline_cache::cycle()`（`gpu-cache.cc:1215`）

```cpp
void baseline_cache::cycle() {
  if (!m_miss_queue.empty()) {
    mem_fetch *mf = m_miss_queue.front();
    if (!m_memport->full(mf->size(), mf->get_is_write())) {
      m_miss_queue.pop_front();
      m_memport->push(mf);      // ← 这会调用 icnt_inject_request_packet
    }
  }
  // ...
}
```

- `m_miss_queue.front()` 每个 cycle 只尝试处理 1 个（bandwidth 为 1/cycle）
- 如果 `m_memport->full()` 为 true（interconnect 注入端口已满），则等待，status 保持为 `IN_L1D_MISS_QUEUE`

`simt_core_cluster::icnt_inject_request_packet()`（`shader.cc:4606`）

```cpp
void simt_core_cluster::icnt_inject_request_packet(class mem_fetch *mf) {
  unsigned int packet_size = mf->size();
  if (!mf->get_is_write() && !mf->isatomic())
    packet_size = mf->get_ctrl_size();  // read request = control only（无数据）

  unsigned destination = mf->get_sub_partition_id();  // 已由 addrdec_tlx 决定
  mf->set_status(IN_ICNT_TO_MEM, ...);

  ::icnt_push(m_cluster_id, m_config->mem2device(destination), mf, packet_size);
}
```

- `destination` 是 `mf->get_sub_partition_id() = m_raw_addr.sub_partition`。这是 `mem_fetch` constructor 内部由 `addrdec_tlx()` 对物理地址解码后决定的
- read request 的 packet size = `ctrl_size`（约 40B）而已。返回路径中的数据传输在 reply 侧
- `icnt_push` 在 QV100 中对应 `network_mode=2` → LocalInterconnect_push
- backpressure：`m_memport->full()` → `icnt_has_buffer(destination, size)` → `LocalInterconnect_has_buffer()` 为 false 时阻塞。这是 interconnect 侧的反压点

---

## 4. 从 Interconnect 到 memory_sub_partition

`gpgpu_sim::cycle()` 的 L2 clock 区间（`gpu-sim.cc:2027`）

```cpp
if (clock_mask & L2) {
  for (unsigned i = 0; i < m_n_mem_sub_partition; i++) {
    if (m_memory_sub_partition[i]->full(SECTOR_CHUNCK_SIZE)) {
      gpu_stall_dramfull++;     // ← backpressure counter
    } else {
      mem_fetch *mf = (mem_fetch *)icnt_pop(m_shader_config->mem2device(i));
      m_memory_sub_partition[i]->push(mf, cycle);  // ← 即使是 NULL 也会调用
    }
    m_memory_sub_partition[i]->cache_cycle(cycle);
  }
}
```

- `sub_partition->full()` 返回的是 `m_icnt_L2_queue->full()`。如果满了，`gpu_stall_dramfull` 会增加（这就是 L2 back-pressure）
- 从 interconnect 取出的 `mf` 会立刻传给 `sub_partition`

`memory_sub_partition::push()`（`l2cache.cc:786`）

```cpp
void memory_sub_partition::push(mem_fetch *m_req, unsigned long long cycle) {
  if (m_req) {
    m_stats->memlatstat_icnt2mem_pop(m_req);  // icnt2mem latency 统计

    std::vector<mem_fetch*> reqs;
    if (m_config->m_L2_config.m_cache_type == SECTOR)
      reqs = breakdown_request_to_sector_requests(m_req);  // sector 分割
    else
      reqs.push_back(m_req);

    for (auto req : reqs) {
      m_request_tracker.insert(req);
      if (req->istexture()) {
        m_icnt_L2_queue->push(req);
        req->set_status(IN_PARTITION_ICNT_TO_L2_QUEUE, ...);
      } else {
        rop_delay_t r;
        r.ready_cycle = cycle + m_config->rop_latency;  // QV100: 160 cycles
        r.req = req;
        m_rop.push(r);
        req->set_status(IN_PARTITION_ROP_DELAY, ...);
      }
    }
  }
}
```

- non-texture access（包括 global memory）一定会经过 ROP delay queue
- QV100 中 `rop_latency = 160 cycles`（配置文件：`gpgpu_l2_rop_latency 160`）
- 这 160 cycles 是 `averagemflatency = 211 cycles` 中最大的固定成本
- ROP delay timer 到期后，会 push 到 `m_icnt_L2_queue` → `IN_PARTITION_ICNT_TO_L2_QUEUE`

---

## 5. L2 cache access 与 m_L2_dram_queue

`memory_sub_partition::cache_cycle()`（`l2cache.cc:465`）— 4 个阶段的处理

`cache_cycle` 每个 cycle 按以下顺序执行（虽然源码中可能以反向顺序书写，但逻辑上对应如下）：

### ① L2 fill response 的排出（`l2cache.cc:467`）

```cpp
if (m_L2cache->access_ready() && !m_L2_icnt_queue->full()) {
  mem_fetch *mf = m_L2cache->next_access();
  mf->set_reply();
  mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE, ...);
  m_L2_icnt_queue->push(mf);
}
```

→ 将 L2 fill 完成的 `mf` 送入 `m_L2_icnt_queue`

### ② DRAM → L2 fill（`l2cache.cc:494`）

```cpp
if (!m_dram_L2_queue->empty()) {
  mem_fetch *mf = m_dram_L2_queue->top();
  if (m_L2cache->waiting_for_fill(mf)) {
    if (m_L2cache->fill_port_free()) {
      mf->set_status(IN_PARTITION_L2_FILL_QUEUE, ...);
      m_L2cache->fill(mf, cycle);
      m_dram_L2_queue->pop();
    }
  } else if (!m_L2_icnt_queue->full()) {
    m_L2_icnt_queue->push(mf);   // texture 情况等
    m_dram_L2_queue->pop();
  }
}
```

### ③ L2 cache cycle（`l2cache.cc:514`）

```cpp
m_L2cache->cycle();  // baseline_cache::cycle() - miss_queue drain
```

### ④ icnt → L2 access（`l2cache.cc:517`）

```cpp
if (!m_L2_dram_queue->full() && !m_icnt_L2_queue->empty()) {
  mem_fetch *mf = m_icnt_L2_queue->top();
  enum cache_request_status status = m_L2cache->access(mf->get_addr(), mf, cycle, events);

  if (status == HIT) {
    mf->set_reply();
    mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE, ...);
    m_L2_icnt_queue->push(mf);
    m_icnt_L2_queue->pop();
  } else if (status != RESERVATION_FAIL) {
    // MISS/SECTOR_MISS: L2cache 内部的 miss_queue → L2interface::push() 后进入 m_L2_dram_queue
    m_icnt_L2_queue->pop();
  }
  // RESERVATION_FAIL: retry next cycle
}
```

`L2interface::push()`（`l2cache.h:260`）

```cpp
virtual void push(mem_fetch *mf) {
  mf->set_status(IN_PARTITION_L2_TO_DRAM_QUEUE, 0);
  m_unit->m_L2_dram_queue->push(mf);
}
```

→ L2 miss 的 `mf` 状态变为 `IN_PARTITION_L2_TO_DRAM_QUEUE`，并进入 `m_L2_dram_queue`

### L1D 与 L2 的差异

| 项目 | L1D | L2 |
|---|---|---|
| 类型 | `data_cache / sector_cache` | `l2_cache`（同样的基类） |
| Miss queue 出口 | `m_memport → icnt_push → interconnect` | `L2interface → m_L2_dram_queue` |
| ROP delay | 无 | 有（160 cycles） |
| Fill 来源 | L2 (`m_dram_L2_queue → L2 fill`) | DRAM (`m_dram_L2_queue`) |

### baseline 的数值对应

- `L2_total_cache_accesses = 96` → `m_L2cache->access()` 被调用了 96 次
- `L2_total_cache_misses = 32` → 其中 32 件为 MISS → 进入 `m_L2_dram_queue`
- `miss_rate = 0.3333`

---

## 6. DRAM scheduling / timing / return queue

`memory_partition_unit::dram_cycle()`（`l2cache.cc:306`）

`dram_cycle()` 在 1 个 cycle 中的动作：

1. `m_dram->return_queue_top()` → `dram_L2_queue_push(mf)` `[IN_PARTITION_DRAM_TO_L2_QUEUE]`
2. `m_dram->cycle()` → scheduler + bank commands + `rwq`
3. `L2_dram_queue` → `dram_latency_queue`（ready cycle = now + 100）`[IN_PARTITION_DRAM_LATENCY_QUEUE]`
4. `dram_latency_queue.front().ready_cycle <= now` → `m_dram->push(mf)` `[IN_PARTITION_MC_INTERFACE_QUEUE]`

- `dram_latency = 100 cycles`（配置文件中设定）是“从 L2 到 DRAM controller 的固定延迟”。这和真正的 DRAM timing 不同，是进入 DRAM 之前的 pre-queue 延迟
- backpressure：`can_issue_to_dram(spid) = has_credits() && !dram_L2_queue_full()`。如果 DRAM return queue 或 scheduling queue 堵塞，会产生 back-pressure

`dram_t::push()`（`dram.cc:247`）

```cpp
data->set_status(IN_PARTITION_MC_INTERFACE_QUEUE, ...);
mrqq->push(dram_req_t*)  // mrqq = small FIFO (depth 2) → 交给 scheduler
```

`dram_t::scheduler_frfcfs()`（`dram_sched.cc:207`）

```cpp
// 将所有 mrqq drain 到 FR-FCFS queue
while (!mrqq->empty()) {
  req = mrqq->pop();
  req->data->set_status(IN_PARTITION_MC_INPUT_QUEUE, ...);
  sched->add_req(req);   // 添加到 frfcfs_scheduler
}

// 对每个 bank 执行 schedule()
for (each bank b) {
  if (!bk[b]->mrq) {
    req = sched->schedule(b, bk[b]->curr_row);  // FR-FCFS 选择
    req->data->set_status(IN_PARTITION_MC_BANK_ARB_QUEUE, ...);
    bk[b]->mrq = req;
  }
}
```

`frfcfs_scheduler::schedule()`（`dram_sched.cc:109`）

FR-FCFS 的选择逻辑：

1. 如果存在对当前 active row 的访问 → Row Hit（优先）
2. 否则 → queue 中的 OLDEST request（FCFS）→ 选择新的 row（通过 `data_collection()` 更新统计）

`dram_t::issue_col_command()`（`dram.cc:555`）

```cpp
bk[j]->mrq->data->set_status(IN_PARTITION_DRAM, ...);  // 每个 cycle，只要存在 mrq 就会更新
if (!CCDc && !bk[j]->RCDc && curr_row == mrq->row && rw == READ && WTRc == 0 && BANK_ACTIVE && !rwq->full()) {
  rwq->push(bk[j]->mrq);  // 发出 column read
  CCDc = tCCD; RTWc = tRTW; ...
}
```

- `rwq = fifo_pipeline("rwq", CL, CL+1)` → 固定 CL=12 cycles 的 pipeline。`rwq->pop()` 会取出已完成的 command

`dram_t::cycle()` 的 return（`dram.cc:290`）

```cpp
if (!returnq->full()) {
  dram_req_t *cmd = rwq->pop();   // CL cycles 后可以 pop
  if (cmd && cmd->dqbytes >= m_config->busW) {
    data->set_status(IN_PARTITION_MC_RETURNQ, ...);
    data->set_reply();
    returnq->push(data);   // returnq = dram return FIFO
  }
}
```

### DRAM 延迟的构成（QV100 config）

| 区间 | 延迟 | 备注 |
|---|---:|---|
| `dram_latency_queue` | 100 cycles | L2→DRAM 固定延迟（pre-queue） |
| `mrqq → IN_PARTITION_MC_INPUT_QUEUE` | ~0-2 cycles | FIFO depth 2 |
| FR-FCFS queue | 可变 | 依赖拥塞程度。`vecadd` 中负载较低 |
| Row buffer miss: ACT (`tRCD=12`) + RAS (`tRAS=28`) | ~28-40 cycles |  |
| CL pipeline (`rwq` depth = CL=12) | 12 cycles | column read latency |
| 合计（row miss 时的推定） | 100 + ~52 = ~152 cycles |  |

---

## 7. L2 fill 与返回路径 interconnect

### DRAM → L2 fill

在 `dram_cycle()` 中，`return_queue_top()` → `dram_L2_queue_push(mf)` → `IN_PARTITION_DRAM_TO_L2_QUEUE`

在 `cache_cycle()` 的阶段 ② 中：

```cpp
m_L2cache->waiting_for_fill(mf) → true 的情况
mf->set_status(IN_PARTITION_L2_FILL_QUEUE, ...)
m_L2cache->fill(mf, cycle)
```

`baseline_cache::fill()` 被调用后，MSHR entry 被满足，`access_ready()` 变为 true。

### L2 fill 完成 → m_L2_icnt_queue

在 `cache_cycle()` 的阶段 ① 中：

```cpp
mf = m_L2cache->next_access()   // fill 完成的 mf
mf->set_reply()                 // type: READ_REQUEST → READ_REPLY
mf->set_status(IN_PARTITION_L2_TO_ICNT_QUEUE, ...)
m_L2_icnt_queue->push(mf)
```

### m_L2_icnt_queue → interconnect

`gpgpu_sim::cycle()` 的 ICNT clock（`gpu-sim.cc:1981`）：

```cpp
mem_fetch *mf = m_memory_sub_partition[i]->top();  // m_L2_icnt_queue->top()
unsigned response_size = mf->get_is_write() ? ctrl_size : mf->size();
if (icnt_has_buffer(mem2device(i), response_size)) {
  mf->set_return_timestamp(cycle);  // 设置 m_timestamp2（用于 icnt2sh 统计）
  mf->set_status(IN_ICNT_TO_SHADER, ...);
  icnt_push(mem2device(i), mf->get_tpc(), mf, response_size);
  m_memory_sub_partition[i]->pop();  // m_L2_icnt_queue->pop + request_tracker->erase
} else {
  gpu_stall_icnt2sh++;  // 返回路径 interconnect 满时的计数器
}
```

- 注意：返回 packet 是 read reply，因此包含数据。`mf->size()` = data_size + ctrl_size
- `gpu_stall_icnt2sh` 是用于测量返回路径 interconnect backpressure 的统计项

---

## 8. shader core 接收 response → ldst_unit::writeback

`simt_core_cluster::icnt_cycle()`（`shader.cc:4712`）

```cpp
// ① 先处理 m_response_fifo 头部
if (!m_response_fifo.empty()) {
  mem_fetch *mf = m_response_fifo.front();
  unsigned cid = m_config->sid_to_cid(mf->get_sid());
  if (/* data access */) {
    if (!m_core[cid]->ldst_unit_response_buffer_full()) {
      m_response_fifo.pop_front();
      m_memory_stats->memlatstat_read_done(mf);   // ← averagemflatency 的终点
      m_core[cid]->accept_ldst_unit_response(mf); // → ldst_unit::fill(mf)
    }
  }
}

// ② 从 interconnect 接收新到达的 mf
if (m_response_fifo.size() < n_simt_ejection_buffer_size) {
  mem_fetch *mf = ::icnt_pop(m_cluster_id);
  if (mf) {
    mf->set_status(IN_CLUSTER_TO_SHADER_QUEUE, ...);
    m_response_fifo.push_back(mf);
  }
}
```

- 两级 FIFO：`m_response_fifo`（cluster 的 ejection buffer）→ `ldst_unit::m_response_fifo`
- `ldst_unit_response_buffer_full()` = `m_response_fifo.size() >= ldst_unit_response_queue_size`

`ldst_unit::fill()`（`shader.cc:2333`）

```cpp
void ldst_unit::fill(mem_fetch *mf) {
  mf->set_status(IN_SHADER_LDST_RESPONSE_FIFO, ...);
  m_response_fifo.push_back(mf);
}
```

`ldst_unit::cycle()`（`shader.cc:2834`）— 对 `m_response_fifo` 的处理

```cpp
if (!m_response_fifo.empty()) {
  mem_fetch *mf = m_response_fifo.front();
  // global/local read (L1D enabled):
  if (m_L1D->fill_port_free()) {
    m_L1D->fill(mf, cycle);  // 向 L1D fill data
    m_response_fifo.pop_front();
  }

  // global/local read (L1D bypass 或 gmem_skip_L1D):
  if (m_next_global == NULL) {
    m_response_fifo.pop_front();
    m_next_global = mf;      // → 由 writeback client 3 处理
  }
}
```

`ldst_unit::writeback()`（`shader.cc:2692`）— `releaseRegister` 的条件

```cpp
void ldst_unit::writeback() {
  if (!m_next_wb.empty()) {
    if (m_operand_collector->writeback(m_next_wb)) {
      for (unsigned r = 0; r < MAX_OUTPUT_VALUES; r++) {
        if (m_next_wb.out[r] > 0) {
          unsigned reg_id = ...;
          --m_pending_writes[m_next_wb.warp_id()][m_next_wb.out[r]];
          if (!still_pending) {
            m_pending_writes[...].erase(...);
            m_scoreboard->releaseRegister(warp_id, reg_id);  // ← 这里是最终点
          }
        }
      }
    }
  }

  // writeback client round-robin:
  // client 3 = global: m_next_global → m_next_wb
  // client 4 = L1D: m_L1D->access_ready() → next_access() → m_next_wb
}
```

### 为什么要“等待所有 mem_fetch 返回”

一条 warp load instruction（例如：32 thread × 4B = 128B）在 coalescing 后可能生成 N 个 `mem_fetch`：

- `execute()` → `memory_cycle()` 中执行 `m_pending_writes[warp_id][reg] += N`（`shader.cc:2677`）
- 每个 `mf` 经过 `writeback()` 时都会 `--m_pending_writes`
- 只有最后一个 `mf`（still_pending == 0）到达时，才会调用 `releaseRegister()`

因此，scoreboard 会持续 hold register，直到最后一个 data 返回。

Day1 观察到的 `W0_Scoreboard = 1154 cycles`，是从第一个 `mem_fetch` 进入 `m_miss_queue` 到最后一个 `mem_fetch` 到达 `releaseRegister()` 的完整时间段。其中包含 `averagemflatency = 211 cycles` 对应的 memory latency（scoreboard stall 更长：从 issue 到全部 fetch 完成）。

---

## 9. mem_fetch status 生命周期

通过代码确认的实际状态迁移链：

```text
MEM_FETCH_INITIALIZED
└── (L1D miss: baseline_cache::access() → m_miss_queue.push_back)
IN_L1D_MISS_QUEUE
└── (baseline_cache::cycle(): !m_memport->full() → pop + push)
    (simt_core_cluster::icnt_inject_request_packet())
IN_ICNT_TO_MEM
└── (gpgpu_sim::cycle() L2 clock: icnt_pop → sub_partition::push())
    (非 texture → rop_delay_t queue: ready_cycle = now + 160)
IN_PARTITION_ROP_DELAY
└── 160 cycles 固定
    (cache_cycle() 的 ROP 到期 → m_icnt_L2_queue)
IN_PARTITION_ICNT_TO_L2_QUEUE
└── (cache_cycle() → m_L2cache->access())
    HIT → set_reply + m_L2_icnt_queue
    MISS → L2interface::push → m_L2_dram_queue
IN_PARTITION_L2_TO_DRAM_QUEUE    [L2 MISS 时]
└── (dram_cycle(): L2_dram_queue_pop → dram_latency_queue)
IN_PARTITION_DRAM_LATENCY_QUEUE
└── dram_latency = 100 cycles 固定
    (dram_cycle(): ready → m_dram->push())
IN_PARTITION_MC_INTERFACE_QUEUE
└── (scheduler_frfcfs(): mrqq drain → add_req())
IN_PARTITION_MC_INPUT_QUEUE
└── (scheduler_frfcfs(): schedule() → bk[b]->mrq = req)
IN_PARTITION_MC_BANK_ARB_QUEUE
└── (issue_col/rw_command(): timing 制约检查中也会更新)
IN_PARTITION_DRAM
└── (dram_t::cycle(): rwq->pop() + dqbytes >= nbytes)
IN_PARTITION_MC_RETURNQ
└── (dram_cycle(): return_queue_top() → dram_L2_queue_push)
IN_PARTITION_DRAM_TO_L2_QUEUE
└── (cache_cycle(): waiting_for_fill → m_L2cache->fill())
IN_PARTITION_L2_FILL_QUEUE
└── (cache_cycle(): access_ready → next_access → set_reply → m_L2_icnt_queue)
IN_PARTITION_L2_TO_ICNT_QUEUE
└── (gpgpu_sim ICNT clock: top → icnt_has_buffer → set_return_timestamp → icnt_push)
IN_ICNT_TO_SHADER
└── (simt_core_cluster::icnt_cycle(): icnt_pop → m_response_fifo)
IN_CLUSTER_TO_SHADER_QUEUE
└── (icnt_cycle(): accept_ldst_unit_response → ldst_unit::fill)
    ← memlatstat_read_done() 在这里作为 averagemflatency 终点
IN_SHADER_LDST_RESPONSE_FIFO
└── (ldst_unit::cycle(): L1D->fill or m_next_global = mf)
IN_SHADER_FETCHED
└── (writeback(): --m_pending_writes; if 0 → releaseRegister())
→ DELETE
```

如果是 L2 HIT，会跳过 `IN_PARTITION_L2_TO_DRAM_QUEUE` 之后的全部 DRAM 阶段，直接进入 `IN_PARTITION_L2_TO_ICNT_QUEUE`。

---

## 10. baseline 数值到 Day3 机制的映射

| 统计值 | 值 | Day3 中的对应 |
|---|---:|---|
| `averagemflatency` | 211 cycles | mf 生成 → `accept_ldst_unit_response()`。ROP 160 + interconnect + L2/DRAM |
| `L2_total_cache_accesses` | 96 | `m_L2cache->access()` 调用次数 |
| `L2_total_cache_misses` | 32 | 进入 `m_L2_dram_queue` 的件数 |
| `L2_total_cache_miss_rate` | 0.3333 | 32/96 |
| `gpu_tot_sim_cycle` | 5569 | 整个 kernel 的 simulation cycle |
| `L1D miss rate` | 1.000 | cold cache → 所有 load 都进入 `m_miss_queue` |

### 211 cycles 的 rough 内部构成（推定）

- ROP delay: 160 cycles（所有 non-texture 访问共用，已通过代码确认）
- interconnect + queue 累积：~5-10 cycles
- L2 HIT（2/3）：额外约 ~0-5 cycles
- L2 MISS（1/3）：DRAM latency queue（100 cycles）+ FR-FCFS wait（低负载下几乎为 0）+ row miss ACT（~28 cycles）+ CL（12 cycles）= 额外 140 cycles
- Weighted: 0.333 × 140 + 0.667 × ~3 ≈ 49 cycles 的 DRAM 追加平均
- 合计推定：160 + ~8 + ~49 ≈ ~217 cycles（接近实测 211）

→ ROP delay（160 cycles）约占 76%，是支配性因素。这是 `vecadd` 这类 global memory access 始终要承担的固定成本。

---

## 11. 后续 cache/MMU 改动的插入点

### A. L2 replacement / bypass / write allocate

- 插入文件：`src/gpgpu-sim/gpu-cache.cc`（`data_cache::access()`、`baseline_cache::access()`）
- 插入位置：`cache_cycle()` 的 ④ 中，调用 `m_L2cache->access()` 前后
- 统计：`L2_total_cache_miss_rate`, `L2_total_cache_accesses`

### B. L2 MSHR / queue / sub-partition backpressure

- 插入文件：`src/gpgpu-sim/l2cache.cc`（`memory_sub_partition::cache_cycle()`）
- `RESERVATION_FAIL` 时会 retry。修改 MSHR size 会影响 `IN_PARTITION_ICNT_TO_L2_QUEUE` 中的 stall
- `m_icnt_L2_queue->full()` → `gpu_stall_dramfull`
- 统计：`gpu_stall_dramfull`

### C. interconnect bandwidth / packet size / routing

- 插入文件：`src/gpgpu-sim/icnt_wrapper.cc` 或 `src/intersim2/`
- Packet size：修改 `icnt_inject_request_packet()` 中的 `packet_size`
- QV100 使用 LocalInterconnect（`network_mode=2`）。改为 BookSim 时会切换到 `intersim2`
- 统计：`gpu_stall_icnt2sh`, `n_simt_to_mem`, `n_mem_to_simt`

### D. DRAM scheduler / bank mapping / row buffer policy

- DRAM scheduler：`src/gpgpu-sim/dram_sched.cc`（在 `frfcfs_scheduler::schedule()` 中加入自定义策略）
- Bank mapping：`src/gpgpu-sim/dram.cc`（`dram_req_t` constructor 中的 `bk` 计算，dram_bk indexing policy）
- Row buffer policy：`frfcfs_scheduler` 的 rowhit prioritization 逻辑
- 统计：`Row_Buffer_Locality`, `Bank_Level_Parallism`, `gpgpu_n_dram_reads`

### E. TLB/MMU page walk 插入后的 latency 位置

| 插入位置 | 效果 | 代码位置 |
|---|---|---|
| L1D 前（warp issue 时） | 所有 memory access 都增加 page walk latency | `ldst_unit::memory_cycle()` |
| L1D miss 后、icnt 前 | L1D hit 不受影响，只对 miss 增加 | `baseline_cache::cycle()` → `m_memport->push()` 前 |
| L2 前（ROP delay 后） | L2 hit 也会增加 | `cache_cycle()` 的 ④ `m_L2cache->access()` 前 |
| DRAM 前 | 只对 L2 miss 增加 | `dram_cycle()` 的 `L2_dram_queue_pop` 后 |

推荐：现实 GPU TLB 通常与 L1D 处于相近层级。`IN_L1TLB_MISS_QUEUE / IN_VM_MANAGER_QUEUE` 这些状态已经在 `mem_fetch_status.tup` 中定义（`l2cache.cc:34-35`）— 这实际上是研究扩展用的预留 hook。

### F. 效果测定中最适合的统计项

- `averagemflatency` — 端到端总 latency
- `avg_icnt2mem_latency` — shader → memory 侧 interconnect
- `avg_icnt2sh_latency` — memory → shader 侧 interconnect（返回路径）
- `L2_total_cache_miss_rate` — L2 效果测量
- `gpu_stall_dramfull` — L2 backpressure
- `gpu_stall_icnt2sh` — 返回路径 interconnect backpressure
- `Row_Buffer_Locality` — DRAM scheduler 效果
- `gpu_tot_sim_cycle` — 整体性能

---

## 12. Day4 下一步建议

### Day4 候选 A：Warp scheduler 细节

- Day1 中观察到 `W0_Scoreboard = 1154 cycles` 这样很长的 scoreboard stall。Day3 中已经解释了原因（等待所有 `mem_fetch` 返回），但 warp scheduler 如何切换到其他 warp，以及 GTO（Greedy-then-Oldest）等具体实现还没有细读
- 读的函数：`shader_core_ctx::issue_warp()`, `scheduler_unit::order_warps()`, `gto_scheduler`

### Day4 候选 B：cache miss path 细节（sector MSHR, extra_mf_fields）

- `baseline_cache::access()` 中 MSHR / `m_extra_mf_fields` 如何管理
- `data_cache::miss()` → `sector_cache::miss()` 的差异
- 读的函数：`data_cache::access()`, `baseline_cache::access()`, `tag_array::access()`

### Day4 候选 C：TLB/VM 既有 hook 确认

- `IN_L1TLB_MISS_QUEUE`, `IN_VM_MANAGER_QUEUE` 已在 `mem_fetch_status.tup` 中定义
- 确认 `src/gpgpu-sim/shader.cc` 中是否存在 `vm_manager` 相关既有代码
- 具体定位 MMU page walk 研究的插入点

### Day4 候选 D：interconnect 实现细节（LocalInterconnect）

- QV100 config 是 `network_mode=2 = LocalInterconnect`
- `src/gpgpu-sim/icnt_wrapper.cc` 中 `LocalInterconnect_*` 函数族的实现
- bandwidth 限制与 backpressure 的准确机制

---

以上是 Day3 的阅读报告。可以确认：ROP delay（160 cycles）是 `averagemflatency = 211 cycles` 的支配性因素，并且完整状态迁移链（约 20 个状态）已经确认。

---

# Claude's plan

## Day3 Reading Session — L2 / Interconnect / DRAM / Return Path

This is a read-only analysis session. No code changes planned.

### Files Read

- `src/gpgpu-sim/mem_fetch.h / mem_fetch.cc`
- `src/gpgpu-sim/mem_fetch_status.tup`
- `src/gpgpu-sim/l2cache.h / l2cache.cc`
- `src/gpgpu-sim/dram.h / dram.cc`
- `src/gpgpu-sim/dram_sched.cc`
- `src/gpgpu-sim/gpu-sim.cc`（lines 1970-2060）
- `src/gpgpu-sim/shader.cc`（icnt_inject, icnt_cycle, ldst_unit::fill, writeback）
- `src/gpgpu-sim/mem_latency_stat.cc`
- `configs/tested-cfgs/SM7_QV100/gpgpusim.config`

### Key Config（SM7_QV100）

- `m_n_mem = 32 partitions`, `m_n_sub_partition_per_memory_channel = 2` → 64 sub_partitions
- `rop_latency = 160 cycles`
- `dram_latency = 100 cycles`（latency queue before `dram_t::push`）
- `network_mode = 2`（LocalInterconnect）
- DRAM scheduler = FRFCFS
- DRAM timing: `tRCD=12`, `tRAS=28`, `CL=12`, `tRP=12`
- `gpgpu_frfcfs_dram_sched_queue_size = 64`
- `gpgpu_dram_return_queue_size = 192`

### Full Status Chain

```text
MEM_FETCH_INITIALIZED → IN_L1D_MISS_QUEUE → IN_ICNT_TO_MEM →

    MEM_FETCH_INITIALIZED → IN_L1D_MISS_QUEUE → IN_ICNT_TO_MEM →
    IN_PARTITION_ROP_DELAY → IN_PARTITION_ICNT_TO_L2_QUEUE →
    (L2 MISS →) IN_PARTITION_L2_TO_DRAM_QUEUE →
    IN_PARTITION_DRAM_LATENCY_QUEUE → IN_PARTITION_MC_INTERFACE_QUEUE →
    IN_PARTITION_MC_INPUT_QUEUE → IN_PARTITION_MC_BANK_ARB_QUEUE →
    IN_PARTITION_DRAM → IN_PARTITION_MC_RETURNQ →
    IN_PARTITION_DRAM_TO_L2_QUEUE → IN_PARTITION_L2_FILL_QUEUE →
    IN_PARTITION_L2_TO_ICNT_QUEUE → IN_ICNT_TO_SHADER →
    IN_CLUSTER_TO_SHADER_QUEUE → IN_SHADER_LDST_RESPONSE_FIFO → delete
```

### averagemflatency = 211 cycles Interpretation

- Measured: `mem_fetch` creation → `memlatstat_read_done()` in `simt_core_cluster::icnt_cycle()`
- Dominant contributor: `rop_latency = 160 cycles`（all non-texture accesses）
- L2 miss adds: ~100 cycles（`dram_latency_queue`）+ DRAM timing（~12-40 cycles）
- L2 hit（2/3 of accesses）: ~160 + few cycles
- L2 miss（1/3 of accesses）: ~160 + 100 + DRAM timing

No code changes. Report output directly in chat.
