# Round AF: Pre-Scoreboard Load Gate Implementation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AE 推荐将 gate 移到 `checkCollision()` 之前

---

## 实现内容

将 `ccws_lg_gate_load(wid)` 调用从 scoreboard 通过后（post-scoreboard）移到 scoreboard 检查前（pre-scoreboard）。

**改动位置**：`src/gpgpu-sim/shader.cc`，`scheduler_unit::cycle()`

**改动前**（Round Y，post-scoreboard）：
```
} else {
  valid_inst = true;
  if (!m_scoreboard->checkCollision(warp_id, pI)) {
    ...
    if (LOAD_OP) {
      ccws_load_blocked = ccws_lg_gate_load(warp_id);  // ← 旧位置
      if (!ccws_load_blocked) issue_warp();
    }
  }
}
```

**改动后**（Round AF，pre-scoreboard）：
```
} else {
  // pre-scoreboard gate
  if (enable_ccws && enable_load_gating && is_load_op(pI) &&
      ccws_lg_gate_load(warp_id)) {
    checked++;
    break;  // while 条件 checked(1) <= issued(0) → false → 外层 for 继续
  }
  valid_inst = true;
  if (!m_scoreboard->checkCollision(warp_id, pI)) {
    ...
    if (LOAD_OP) {
      // 旧 gate 代码已移除
      if (m_mem_out->has_free()) issue_warp();
    }
  }
}
```

**改动量**：约 15 行（新增 10 行 pre-scoreboard gate，移除 8 行旧 gate）。

---

## 验证结果

### feature_off 7/7 pass

| workload | sim_cycle | baseline | match |
|----------|-----------|----------|-------|
| rodinia_hotspot | 6931 | 6931 | ✓ |
| strided_access | 5825 | 5825 | ✓ |
| page_stride_access | 5851 | 5851 | ✓ |
| mutual_tiled | 7479 | 7479 | ✓ |
| rodinia_srad_v2 | 15926 | 15926 | ✓ |
| polybench_fdtd2d | 35681 | 35681 | ✓ |
| rodinia_bfs | 136110 | 136110 | ✓ |

### gating_on focused set

| workload | inc | lg_block | cycle | cycle_delta |
|----------|-----|---------|-------|-------------|
| rodinia_hotspot | 5 | 0 | 6931 | 0 |
| rodinia_srad_v2 | 5 | 0 | 15926 | 0 |
| polybench_fdtd2d | 5 | 0 | 35681 | 0 |
| rodinia_hotspot | 20 | 0 | 6931 | 0 |
| rodinia_srad_v2 | 20 | 0 | 15926 | 0 |
| polybench_fdtd2d | 20 | 0 | 35681 | 0 |
| rodinia_hotspot | 30 | 0 | 6931 | 0 |
| rodinia_srad_v2 | 30 | 0 | 15926 | 0 |
| polybench_fdtd2d | 30 | 0 | 35681 | 0 |
| **rodinia_srad_v2** | **50** | **34572** | **23150** | **+7224 (+45%)** |
| **polybench_fdtd2d** | **50** | **454219** | **57411** | **+21730 (+61%)** |

---

## 关键发现：gate 位置移动无效

### 根本原因（新发现）

pre-scoreboard gate 与 post-scoreboard gate 的结果完全相同：
- inc=5/20/30：全部 0 blocks（与 Round AD 相同）
- inc=50：srad_v2 +45%，fdtd2d +61%（与 Round Y 相同）

**原因**：gate 位置不是问题所在。真正的问题是 `would_can_issue` 的计算结果：

inc=20 srad_v2 的 LLS 分布：
- `max_score = 413`
- `sum_score = 550736`（跨 80 SMs）
- `nonzero_warps = 128`（跨 80 SMs，平均每 SM 1.6 个 nonzero warp）
- per-SM cutoff = 64 × 100 = 6400

prefix sum 计算：第一个 warp（max_score=413）的 prefix = 413，远小于 cutoff=6400。所有 warp 的 `would_can_issue=true`。gate 被调用（`lg_attempt=12416`），但全部返回 false（allow）。

**VTA hit 分散在太多 warp 上**，每个 warp 的 LLS 分数都远低于 cutoff/nw（6400/64=100）。只有 inc=50 时，分数积累足够快，使某些 warp 的分数超过 100，prefix sum 才能超过 cutoff。

### 与 Round AE 分析的差异

Round AE 的假设是：高 LLS warp 在 scoreboard stall，gate 无法触发。这个假设是错误的——实际上 `would_can_issue` 全是 true，gate 被调用但全部 allow，与 gate 位置无关。

---

## 新的根本原因分析

| 问题层次 | 描述 |
|---------|------|
| 表面现象 | inc=5/20/30 全部 0 blocks |
| Round AD 假设 | 高 LLS warp 在 scoreboard stall，gate 无法触发 |
| Round AE 假设 | gate 在 post-scoreboard，stall 恢复后 load 仍发射 |
| **实际根因** | `would_can_issue` 全是 true：VTA hit 分散，每个 warp 的 LLS 分数远低于 cutoff/nw |

### 为什么 inc=50 有效

inc=50 时，每次 VTA hit 增加 50 分。srad_v2 有 3924 VTA hits，分布在 ~128 warps（跨 80 SMs）。某些 warp 集中了更多 hits，分数超过 100（cutoff/nw），使 prefix sum 超过 cutoff，`would_can_issue=false`。

---

## 下一步建议

当前 LLS 分数分布问题有两个方向：

**方向 D（降低 threshold）**：将 `lg_score_threshold` 降低到 1–10，使 cutoff = nw × threshold 更小，更容易被 prefix sum 超过。风险：threshold 太低可能 gate 所有 warp（deadlock）。

**方向 E（接受 inc=50，调整 threshold 减少过度 gating）**：inc=50 确实产生 gating 信号，但 +45–61% cycle 过大。尝试 inc=50 + th=150/200 以减少 gating 强度。

**方向 F（重新审视 VTA 近似方案）**：当前 VTA 是 intra-warp 重复 miss 近似，不是论文的 inter-warp eviction-based VTA。论文 VTA 只记录被其他 warp 驱逐的 cache line，信号更集中。当前近似可能导致 hits 过于分散。
