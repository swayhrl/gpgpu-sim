# PCAL Reading Notes

**Round**: AUTO-9 (pcal_reading)
**Date**: 2026-05-01
**Paper source**: Knowledge base (no PDF available in repo)

---

## 论文信息

**标题**：Priority-Based Cache Allocation in Throughput Processors
**作者**：Jia, Lowe-Power, Aamodt
**会议**：HPCA 2015
**领域**：GPU cache management / cache bypass / thread priority

---

## 1. 研究背景

### GPU cache 共享压力

GPU throughput processor 上，L1/L2 cache 被同一 SM 内数十个并发 warp 共享。不同 warp 的 cache 访问模式差异极大：

- **Cache-sensitive warp**：有良好时间局部性，miss rate 低，每次缓存的数据会被多次重用
- **Cache-insensitive warp**：局部性差，miss rate 高，大量 cache line 装入后几乎不再被访问 → **cache pollution**

当 cache-insensitive warp 持续替换 cache-sensitive warp 的数据时，后者 hit rate 骤降，带来性能损失。

### 对比已实现论文

| 论文 | 目标问题 | 解法方向 |
|------|---------|---------|
| CCWS | intra-warp cache pressure（locality score） | warp 调度 gating |
| DAWS | inter-warp divergence cache footprint | 发散 warp 节流 |
| PCAL | **inter-warp cache sensitivity分层** | L1 bypass（污染者绕过缓存） |

PCAL 的解法方向与 CCWS/DAWS 不同：不是限制哪些 warp 发射，而是改变谁能**占用 cache 空间**。

---

## 2. PCAL 核心机制

### 2.1 分类（Classification）

对每个 warp（或 CTA）维护一个滑动窗口的 miss rate：

```
miss_rate[warp_id] = miss_count / access_count   （最近 W 次访问）
```

- `miss_rate > threshold` → **低优先级**（cache-insensitive）→ bypass
- `miss_rate ≤ threshold` → **高优先级**（cache-sensitive）→ 正常缓存

窗口大小 W 和阈值 threshold 均可配置。

### 2.2 Bypass（缓存绕过）

对低优先级 warp 的 load 指令：**不分配 L1 cache line，直接发往互联网络（去 L2 / DRAM）**。

在 GPGPU-Sim 中，bypass 路径已存在（`bypassL1D` 变量）：
- `shader.cc:2463`：出发路径——bypassL1D=true 时跳过 L1D，直接 push 到 `m_icnt`
- `shader.cc:3087`：返回路径——bypassL1D=true 时放入 `m_next_global`，不 fill L1D

PCAL 的新增条件：**在上述两个 bypassL1D 判断中加入 per-warp 优先级检查**。

### 2.3 自适应更新（Adaptive Update）

- 每隔 window_size 周期重置 miss rate 计数器
- 分类可动态更新：若某 warp 的局部性改善，可升回高优先级

近似实现（本 repro）：**简单滑动窗口，无复杂状态机**。

---

## 3. GPGPU-Sim 源码映射

### 3.1 关键文件

| 文件 | 用途 |
|------|------|
| `src/gpgpu-sim/shader.h` | per-warp 状态（miss/access counter arrays）；config knobs |
| `src/gpgpu-sim/shader.cc` | bypass 决策点；L1D miss/hit 事件 hook |
| `src/gpgpu-sim/gpu-sim.cc` | 全局 stat 收集；config option_parser_register |
| `src/gpgpu-sim/gpu-cache.h` | cache_block_state、cache_operator_type（CACHE_GLOBAL 等） |

### 3.2 已存在的 bypass 路径

**出发路径（shader.cc:2460–2494）**：
```cpp
bool bypassL1D = false;
if (CACHE_GLOBAL == inst.cache_op || (m_L1D == NULL)) {
    bypassL1D = true;
} else if (inst.space.is_global()) {
    if (m_core->get_config()->gmem_skip_L1D && (CACHE_L1 != inst.cache_op))
        bypassL1D = true;
}
// PCAL 将在此处加入：
// if (pcal_should_bypass(inst.warp_id())) bypassL1D = true;
```

**返回路径（shader.cc:3087–3095）**：
```cpp
bool bypassL1D = false;
if (CACHE_GLOBAL == mf->get_inst().cache_op || (m_L1D == NULL)) {
    bypassL1D = true;
} else if (...gmem_skip_L1D...) {
    bypassL1D = true;
}
// PCAL 将在此处加入：
// if (pcal_should_bypass_fill(mf->get_wid())) bypassL1D = true;
```

### 3.3 Miss 事件挂钩点（shader.cc:2213–2218）

```cpp
} else {
    assert(status == MISS || status == HIT_RESERVED);
    // 已有 CCWS VTA probe hook：
    if (m_config->gpgpu_enable_ccws) {
        ccws_vta_probe_miss(inst.warp_id(), ba);
    }
    // PCAL miss counter 将在此处加入：
    // pcal_record_miss(inst.warp_id());
    inst.accessq_pop_back();
}
```

同理，access 事件（包括 HIT + MISS 两路）用于分母计数。

### 3.4 Config 结构位置

`shader.h:1804` 附近（DAWS knobs 参考位置）：CCWS 21 个 knob，DAWS 7 个 knob，PCAL 预计 6 个 knob。

---

## 4. 关键问题与近似决策

### Q1：分类粒度？

论文原版可能是 per-CTA 或 per-warp。本复现选 **per-warp**（与 CCWS/DAWS 一致，实现更简单）。

### Q2：miss rate 的分母是什么？

- 选择 A：仅统计 global memory access（与 `inst.space.is_global()` 对齐）
- 选择 B：统计所有 L1D access

**本复现选 A**（global memory access，与 bypass 判断条件一致）。

### Q3：bypass 是否包含 store？

原论文可能只 bypass load（store 一般是 write-evict 不走 L1D）。本复现 **only bypass load**（`inst.is_load()`）。

### Q4：window 重置时机？

每 `pcal_window_size` 个 access（而非 cycle），避免 idle warp 的计数器永不重置。

### Q5：fill 路径 bypass？

当原始 load 已 bypass L1D 时，返回的 response 自然不填 L1D（在 GPGPU-Sim 中，bypass 出发路径直接 push 到 ICNT，response 回来时走 m_next_global，不经过 fill path）。

**→ 实际上只需修改出发路径（shader.cc:2463 处），fill 路径无需额外修改。**

---

## 5. 实现风险评估

| 风险 | 等级 | 说明 |
|------|------|------|
| bypass 破坏 coherence | 中 | L1D 是 read-only write-evict，bypass 不影响一致性 |
| per-warp miss counter 数组越界 | 低 | 与 DAWS `m_daws_footprint[]` 使用相同 pattern |
| 死锁（所有 warp bypass → warp stall） | 低 | bypass 路径 ICNT 满时 stall，不是死锁；warp 仍可发射 |
| window 重置导致分类抖动 | 中 | threshold 可配置缓解 |
| **cache module 侵入性改动** | **高** | gpu-cache.cc 复杂，bypass 在 shader.cc 已有路径，不需进入 gpu-cache.cc |

**最高风险改动**：在 L1D access 路径中加入 per-warp bypass gate，需要确保 `inst.warp_id()` 在此上下文中有效（通过 DAWS 的先例已知可行）。

---

## 6. 与现有实现的差异

本分支继承自 `hrl/paper/daws-repro-v0`（含 CCWS + DAWS instrumentation），所有 PCAL knob 默认 0/off，不与 CCWS/DAWS 机制冲突。

- CCWS：`gpgpu_enable_ccws=0` → PCAL 与 CCWS 互不干涉
- DAWS：`gpgpu_enable_daws=0` → 同上
- feature_off 验证标准：`gpgpu_enable_pcal=0` 时 sim_cycle = 5569，所有 `paper_pcal_*` = 0

---

## 7. 参考资料

- 论文：Jia et al., "Priority-Based Cache Allocation in Throughput Processors," HPCA 2015
- GPGPU-Sim bypass 路径：`shader.cc:2463`（出发）、`shader.cc:3087`（返回）
- CCWS miss probe 模式：`shader.cc:1925–1959`（ccws_vta_probe_miss）
- DAWS per-warp 状态 pattern：`shader.h:2251–2330`
