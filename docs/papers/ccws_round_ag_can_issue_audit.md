# Round AG: Can-Issue Cutoff and Active-Warp Set Audit

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AF 确认 gate 位置不是问题；`would_can_issue` 几乎全 true

---

## 1. 当前实现审计

### would_can_issue 计算代码（shader.cc:3170–3189）

```cpp
unsigned nw = m_ccws_lls.size();                          // = max_warps_per_shader = 64
unsigned long long cum_cutoff = (unsigned long long)nw    // 64
    * m_config->gpgpu_ccws_lg_score_threshold;            // × 100 = 6400
// sort all 64 warp slots by LLS score (descending)
// prefix sum: would_can_issue[wid] = (prefix <= cum_cutoff)
```

**关键问题**：
- `nw = m_ccws_lls.size() = max_warps_per_shader = 64`（SM7_QV100 每 SM 最大 warp 数）
- `cum_cutoff = 64 × 100 = 6400`
- prefix sum 遍历所有 64 个 warp slot，包括 inactive warp

### LLS 初始化

```cpp
m_ccws_lls.assign(config->max_warps_per_shader, config->gpgpu_ccws_lls_base_score);
// 所有 64 个 slot 初始化为 base_score = 100
```

inactive warp 的 LLS 永远保持 `base_score = 100`（decay 下限 = base_score，VTA hit 只在 L1D miss 时触发）。

---

## 2. 实际 Active Warp 数量

| workload | occupancy | active_warps/SM | current_cutoff | correct_cutoff |
|----------|-----------|-----------------|----------------|----------------|
| rodinia_srad_v2 | 12.4% | ~8 | 6400 | 800 |
| rodinia_hotspot | 12.3% | ~8 | 6400 | 800 |
| polybench_fdtd2d | 11.6% | ~7 | 6400 | 700 |

**cutoff 过大倍数**：6400 / 800 = **8×**（与 max/active warp 比例一致）。

---

## 3. 根本 Bug：inactive warp 消耗 cutoff budget

prefix sum 的精确分析（以 srad_v2 inc=20 per-SM 为例）：

- inactive warps：56 个，score = 100 each
- active warps：8 个，max_score = 413
- 排序后（降序）：[413, ~100×7 (active), 100×56 (inactive)]
- prefix[0] = 413 < 6400 → can_issue = true
- prefix[0..7] ≈ 413 + 7×100 = 1113 < 6400 → **所有 active warp can_issue = true**
- prefix[0..62] ≈ 1113 + 55×100 = 6613 > 6400 → 最后几个 inactive warp can_issue = false

**`would_can_issue=false` 只落在 inactive warp slot 上**，这些 warp 不会发射 load，gate 永远不触发。

### 数值验证

inactive warp 的 base_score 合计 = 56 × 100 = 5600，占 cutoff(6400) 的 **87.5%**。active warp 的额外 score（最多 413）只能填满剩余 800 的一部分。prefix sum 在遍历完所有 active warp 后仍远小于 6400，直到遍历到 inactive warp 才超过。

---

## 4. 论文语义对比

| 方面 | 论文 CCWS | 当前实现 |
|------|----------|---------|
| `NumActiveWarps` | 当前 SM 实际运行的 warp 数（~8） | `max_warps_per_shader`（64） |
| `CumLLSCutoff` | `NumActiveWarps × BaseScore`（~800） | `max_warps_per_shader × threshold`（6400） |
| prefix sum 范围 | 只遍历 active warp | 遍历所有 64 个 warp slot |
| `would_can_issue=false` 落点 | 低 LLS 的 active warp | inactive warp slot（不发射 load） |

论文 Section 3.3：
> "CumLLSCutoff = NumActiveWarps × BaseScore"
> "NumActiveWarps is the number of warps currently active in the SM"

---

## 5. 为什么 inc=50 有效

inc=50 时，active warp 的 LLS 分数积累极快（每次 VTA hit +50）。某些 active warp 的分数超过 100，使 prefix sum 在遍历完 active warp 之前就超过 6400：

- 若某 active warp score = 1000，prefix[0] = 1000 < 6400 → can_issue = true
- 但 prefix 增长更快，最终在 active warp 范围内超过 6400
- 此时后续 active warp 的 `would_can_issue=false` → gate 触发

这解释了为什么 inc=50 有效但 inc=5/20/30 无效：inc=50 使 active warp 的分数足够高，使 prefix sum 在 active warp 范围内就超过 6400（而不是等到 inactive warp 才超过）。

---

## 6. 修复方案

**最小修复**：将 `nw` 从 `max_warps_per_shader` 改为 active warp 数，同时只遍历 active warp slot。

```cpp
// 修复前
unsigned nw = m_ccws_lls.size();  // 64

// 修复后（方案一：使用 not_completed / warp_size）
unsigned nw = (m_core->get_not_completed() + m_config->warp_size - 1)
              / m_config->warp_size;
if (nw == 0) nw = 1;  // 防止除零

// 修复后（方案二：只遍历 LLS > base_score 的 warp + 估算 active 数）
// 更复杂，不推荐
```

**同时需要**：prefix sum 只遍历 active warp slot（或只遍历 LLS > 0 的 warp）。

**预期效果**：
- `cum_cutoff = 8 × 100 = 800`（而非 6400）
- prefix sum 在 2-3 个高分 active warp 后就超过 800
- `would_can_issue=false` 落在低 LLS 的 active warp 上
- gate 触发：这些 active warp 有 load 在 ibuffer 中

---

## 7. 是否需要新增诊断 stats

不需要。现有 stats 已足够诊断：
- `gpu_tot_occupancy_pct`：确认 active warp 数
- `paper_ccws_lls_max_score`：确认 LLS 分数范围
- `paper_ccws_lls_nonzero_warps`：确认 nonzero warp 数
- `paper_ccws_load_gate_attempt` vs `load_gate_block`：确认 gate 被调用但全部 allow

---

## 8. 结论

| 问题 | 状态 |
|------|------|
| gate 插入点（Round AE/AF 假设） | 不是根因（已排除） |
| VTA hit 分散（Round AF 假设） | 部分正确，但不是主因 |
| **cutoff 使用 max_warps 而非 active_warps** | **主要 bug** |
| inactive warp base_score 消耗 cutoff budget | **直接原因** |

修复后预期：inc=5/20/30 在 srad_v2/fdtd2d 上出现 `lg_block > 0`，cycle 变化适度（< 10%）。
