# CCWS Reading Notes

_Paper read: 2026-04-30 (Round R). Full text extracted from PDF, 12 pages._

---

## Paper Metadata

| Field | Value |
|-------|-------|
| **Title** | Cache-Conscious Wavefront Scheduling |
| **Authors** | Timothy G. Rogers, Mike O'Connor, Tor M. Aamodt |
| **Venue / Year** | MICRO 2012 (IEEE/ACM Int'l Symp. on Microarchitecture) |
| **Paper key** | `ccws` |
| **Mechanism category** | Cache-conscious warp/wavefront scheduling |
| **Target cache level** | L1D primarily |
| **Likely simulator modules** | scheduler / shader core / ldst_unit / L1D cache feedback / stats |

---

## 研究对象

论文研究 GPU hardware wavefront scheduler（warp 调度器）的调度决策对 L1D cache locality 和 cache thrashing 的影响。

核心问题是：GPU 上同时 active 的 wavefront 数量决定了哪些 memory request 进入 cache——调度器本质上也是一种 cache management 策略，只是通常不被这样看待。

---

## 动机

- GPU 使用大量并发 wavefront 隐藏 memory latency（multithreading）。
- 当 active wavefront 数过多时，它们的 aggregate working set 超过 L1D 容量，导致 **intra-wavefront locality 被破坏**（cache thrashing）。
- Cache-sensitive irregular workload（如 BFS、K-means）的 working set 恰好在 L1D 容量边界附近，thrashing 导致大量 L1D miss，性能严重下降。
- 常规 round-robin / GTO / two-level scheduler 对 lost locality 没有感知，无法自适应。

论文 Figure 3 展示了"performance valley"：IPC 在 active wavefront 数达到 4–7 时达到最优；继续增加 wavefront 数反而会因 thrashing 而使 IPC 下降。

---

## 当前问题

- Cache replacement policy（如 LRU）只能在 cache **内部**的少量候选 line 之间做决策。
- Scheduler 决定哪些 wavefront 的 memory access 进入 cache——影响范围远大于 replacement policy。
- 论文 Figure 8 证明：即使使用 Belady-optimal replacement policy，如果 scheduler 仍然使用 round-robin，性能仍然落后于有良好调度策略的 LRU。
- 结论：**对 HCS workload，调度策略比 replacement policy 更关键。**

---

## 核心思想

CCWS 不修改 cache replacement policy，而是通过 **limiting load-issuing wavefronts** 来减少 cache thrashing：

1. **VTA** 检测某个 wavefront 是否"本可以命中但因为其他 wavefront 的访问把它的数据 evict 掉了"。
2. **LLD** 根据 VTA hit signal 更新该 wavefront 的 **LLS（lost-locality score）**。
3. **LSS** 根据全核 LLS 的排序和 cutoff，确定哪些 wavefront 此刻**不允许 issue load**。
4. 被 gate 的 wavefront 的 non-load 指令仍可正常 issue。

直觉：让"最需要 cache 排他性"的 wavefront 先用 cache，其他 wavefront 等一等。

---

## SWL vs CCWS

| | SWL | CCWS |
|--|-----|------|
| 类型 | 静态 | 动态 |
| 需要 programmer 干预 | 是（launch 时指定 limit） | 否 |
| 需要 offline profiling | 是（每个 workload / input） | 否 |
| 跨 input 泛化 | 差 | 好 |
| 区分 wavefront | 否（统一限额） | 是（按 lost-locality 差异化） |

**重要**：当前 GPGPU-Sim（4.2.0）中**已经存在 `swl_scheduler`** 类，config key 为 `warp_limiting:<prioritization>:<num_warps_to_limit>`（`shader.cc:1678`）。  
Round S 应先 **audit 现有 SWL 实现**，确认其语义是否与论文 SWL 一致，然后将其作为 CCWS 的前置 baseline 使用或隔离。

---

## 机制拆解

### VTA（Victim Tag Array）

- 每个 core 有一个 VTA，按 wavefront ID 分区。
- 每个 wavefront 有固定 entries（论文配置：16 EPW，8-way set-associative，总计 512 entries）。
- VTA **只存 tag，不存数据**。
- 当 L1D 发生 miss/reservation 时，写入 `(tag, WID_of_reserving_warp)` 到 L1D cache line 的附加字段（5-bit）。
- 当 L1D cache line 被 **evict** 时，将 `(tag)` 写入该 WID 对应的 VTA partition。
- 当 L1D 发生 **miss** 时，用当前 `(WID, tag)` probe VTA[WID]。命中则说明该 wavefront 曾将此 line 拥有过，但它被 evict 走了——即 **lost locality** 事件。

**关键实现风险**：当前 GPGPU-Sim 的 `evicted_block_info`（`gpu-cache.h:82`）**不存储 warp_id**，faithful VTA 需要修改 `cache_block_t` 为每个 cache line 附加 WID 字段。

### LLD（Lost Intra-Wavefront Locality Detector）

- 接收 VTA 的 hit signal（附带 WID）。
- 将 VTA hit signal 转发给 Locality Scoring System（LSS）。
- 同时维护两个全局计数器：`VTAHitsTotal` 和 `InstIssuedTotal`。

### LLS（Lost-Locality Score）

- 每个 wavefront 一个 LLS 值，存储在 per-core max-heap 中。
- 初始值 = `BaseLocalityScore`（论文取 100）。
- VTA hit 时：LLS 增加到 LLDS（但 cap 在 LLDS，不继续增加）。
- 每个 cycle：LLS 减 1，直至减回 `BaseLocalityScore`（score decay）。

### LLDS（Lost-Locality Detected Score）

LLDS 是 VTA hit 时分配给该 wavefront 的新分数值：

```
LLDS = (VTAHitsTotal / InstIssuedTotal) × K_THROTTLE × CumLLSCutoff
```

其中 `CumLLSCutoff = NumActiveWaves × BaseLocalityScore`。

LLDS 随核上 VTA hit 密度变化——lost locality 越多，LLDS 越高，wave 被 gate 的时间越长。

### Cumulative LLS Cutoff

```
CumLLSCutoff = NumActiveWaves × BaseLocalityScore
```

- 动态随 active wavefront 数变化。
- LLS 在按值从大到小排序的 max-heap 中，前缀和超过 cutoff 的 wavefront 被认为"低 lost-locality score"，即 **Can Issue = 0**。
- 直觉：cutoff 保证平均每个 wavefront 都贡献一份"公平分"后，超出的低分 wavefront 被 gate。

### Can Issue bit vector

- 每个 wavefront 一个 bit，由 LSS 输出。
- `Can Issue = 0` 时：该 wavefront 不能 issue **load 指令**（non-load 仍可 issue）。
- 在 `scheduler_unit::cycle()` 的 LOAD_OP 分支（`shader.cc:1344`）前加检查即可实现。

### Load-only gating

- CCWS 只 gate load 指令，不影响 store、compute、branch。
- 原因：store 和 compute 不会引入新的 L1D 竞争。

### Score decay

- LLS 每 cycle 减 1，直至 `BaseLocalityScore`。
- 防止 wavefront 被永久 gate——如果 lost locality 停止发生，分数自然下降，gate 自然解除。

### K_THROTTLE 调参

- 论文 Figure 16 展示 K_THROTTLE 从 1 到 128 的扫描。
- **K_THROTTLE = 8** 是 single best static value：对所有 HCS workload 能达到 95.4%–100% 的 per-workload 最优性能。
- CI/MCS workload 几乎不受 K_THROTTLE 影响（因为 VTAHitsTotal 很小）。

---

## 实验方法与 benchmark 分类

| 分类 | 全称 | 缩写 | 备注 |
|------|------|------|------|
| HCS | BFS Graph Traversal | BFS | 高 intra-wavefront locality，不规则 |
| HCS | K-means Clustering | KMN | 修改为使用 global memory |
| HCS | Memcached-GPU | MEMC | server workload，GPU port |
| HCS | Tracing Garbage Collector | GC | server workload，GPU port |
| MCS | Weather Prediction | WP | |
| MCS | StreamCluster | STMCL | |
| MCS | Single Source Shortest Path | SSSP | |
| MCS | CFD Solver | CFD | |
| CI | Needleman-Wunsch | NDL | |
| CI | Back Propagation | BACKP | |
| CI | SRAD | SRAD | |
| CI | LU Decomposition | LUD | |

**原论文使用 GPGPU-Sim 3.1.0**，我们当前版本是 4.2.0。  
原论文 L1D 配置：32KB，128B line，8-way LRU；L2：128KB/channel，128B，8-way LRU，8 channels。  
当前 SM7_QV100 配置与上述不同（Volta 架构），结果数值不可直接对比。  
我们的目标是**复现机制趋势**，不追求完全复制原始 benchmark suite。

我们现有的 HCS-like workload：
- `rodinia_srad_v2`：high L1D miss（L1D=0.79），cache-sensitive stencil
- `page_stride_access`：synthetic HCS stress（cross-page irregular）
- `strided_access`：coalescing degradation + L2 miss spike
- `parboil_spmv`：irregular SpMV
- `rodinia_bfs`：classic irregular BFS（已在 workload 仓库中）

---

## Take-home Message

1. **调度是 cache management**：对 HCS workload，scheduler 对 L1D efficiency 的影响超过 replacement policy。选择 wavefront 的时机 = 选择进入 cache 的 data。

2. **SWL 是 CCWS 的 offline 版本**：两者目标相同（减少 active wavefront 数），区别在于 CCWS 动态感知 lost locality。**当前 GPGPU-Sim 已有 `swl_scheduler`，Round S 应先 audit 它。**

3. **VTA 是轻量级的 lost-locality 检测器**：不存数据，只存 tag；16 entries/warp，512 entries total，area 开销仅 0.17%。

4. **Can Issue gating 只针对 load**：store 和 compute 不受影响，这是正确的，因为只有 load 引入新的 L1D footprint。

5. **K_THROTTLE = 8 是鲁棒的单一静态值**：不需要 per-workload tuning；MCS/CI workload 不受影响。

6. **VTA warp_id 追踪是最大实现风险**：当前 `evicted_block_info` 不带 warp_id。faithful VTA 需要修改 `cache_block_t`。简化版本可只追踪 miss 而非 eviction（精度下降但可先验证机制）。

7. **CCWS 不改 replacement policy**：这是 clean separation of concerns，避免 cache 和 scheduler 两个层面同时有 global state，实现风险可控。

8. **先 feature_off pass，再 feature_on**：`feature_off ≈ baseline` 是第一成功标准，失败则有 instrumentation bug，不可继续实验。
