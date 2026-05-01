# Round AE: Load-Gating Insertion Point Audit

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AD 确认 inc=5/20/30 全部 0 blocks；根因是 gate 只在 issue 阶段触发

---

## 1. 当前实现的精确执行路径

```
scheduler_unit::cycle()
  order_warps()                          ← 构建 m_next_cycle_prioritized_warps（LRR/GTO 顺序）
  for each warp in prioritized_warps:
    while !waiting() && !ibuffer_empty() && checked <= issued:
      pI = ibuffer_next_inst()
      if checkCollision(warp_id, pI):    ← scoreboard 检查（stall warp 在此退出）
        if pI->op == LOAD_OP:
          [Round X] ccws_wg_check_load() ← telemetry only
          [Round Y] ccws_lg_gate_load()  ← 当前 gate 位置
          if !blocked && m_mem_out->has_free():
            issue_warp()
            issued++
      checked++
    if issued: break                     ← 发射成功后跳出外层 for
```

### gate 后的行为

`ccws_load_blocked=true` 时：`issued` 不增加，`checked++`。下一次 while 条件 `checked(1) <= issued(0)` → false → while 退出。`if (issued)` false → 外层 for 继续下一个 warp。

**结论：gate 后确实会继续尝试下一个 warp。** 这部分行为是正确的。

---

## 2. 根本问题：gate 在 scoreboard 检查之后

```
checkCollision() → PASS → [gate 位置]
checkCollision() → FAIL → 直接跳过（scoreboard stall warp 永远不到达 gate）
```

高 LLS warp 的典型状态：
- 刚发射了一条 LOAD_OP，目标寄存器被 scoreboard reserve
- 下一条指令（通常也是 LOAD_OP）在 ibuffer 中等待
- `checkCollision()` 返回 true（有 pending write）→ warp 在 scoreboard 阶段退出
- **gate 永远不会被调用**

这就是为什么 inc=5/20/30 全部 0 blocks：高 LLS warp 的 LLS 分数确实超过了 cutoff，但它们在 scoreboard 阶段就已经被过滤掉，永远不会到达 gate。

---

## 3. inc=50 为何有效（正反馈分析）

inc=50 时，分数积累极快，使 **低 LLS 的非 stall warp** 也超过 cutoff：

- 正常情况：高 LLS warp stall → 低 LLS warp 发射 → 低 LLS warp 的 LLS 也快速增长
- 当低 LLS warp 的分数也超过 cutoff 时，gate 开始触发
- wg_attempt 从 ~16000（inc=30）跳到 470559（inc=50）——增加 28×
- 这说明 inc=50 开始 gate 那些本来可以发射的 warp（非 stall warp）
- 结果：+45–61% cycle（过度 gating）

---

## 4. 论文 Can Issue bit 的正确语义

论文 CCWS（Rogers et al., MICRO 2012）的 Can Issue bit 含义：

> "The Can Issue bit is cleared for warps whose cumulative LLS exceeds the cutoff. When a warp's Can Issue bit is cleared, it is not considered for load instruction issue."

关键词：**not considered for issue**，而不是"到达 issue 阶段后被阻塞"。

论文的 WIA（Wavefront Issue Arbiter）在选择候选 warp 时就已经排除了 Can Issue=0 的 warp（对于 load 指令）。这意味着：

- 高 LLS warp 即使 scoreboard 通过，也不会被选中发射 load
- 低 LLS warp 可以继续发射 load，即使高 LLS warp 在等待

---

## 5. 当前实现 vs 论文语义的差距

| 方面 | 当前实现（Round Y） | 论文语义 |
|------|-------------------|---------|
| gate 位置 | scoreboard 通过后，issue 前 | warp 候选选择阶段（scoreboard 之前） |
| 高 LLS stall warp | 永远不到达 gate | 被 gate 排除，但不影响其他 warp 的 scoreboard 检查 |
| gate 效果 | 只能 gate 非 stall warp | 可以 gate 任何 warp（包括 stall 恢复后的 warp） |
| 实际差异 | 高 LLS warp stall 恢复后，第一条 load 仍会发射 | 高 LLS warp stall 恢复后，load 仍被 gate |

---

## 6. 更合理的插入点分析

### 方案 B1：在 `checkCollision()` 之前插入 gate

```cpp
// 在 checkCollision 之前
if (m_shader->m_config->gpgpu_enable_ccws &&
    m_shader->m_config->gpgpu_ccws_enable_load_gating &&
    pI && (pI->op == LOAD_OP || pI->op == TENSOR_CORE_LOAD_OP) &&
    m_shader->m_ldst_unit->ccws_lg_gate_load(warp_id)) {
  // treat as scoreboard stall: checked++, continue
  checked++;
  continue;  // or break inner while
}
if (!m_scoreboard->checkCollision(warp_id, pI)) { ... }
```

**效果**：高 LLS warp 的 load 在 scoreboard 检查之前就被 gate。stall 恢复后，warp 重新进入候选，但 gate 仍然检查 LLS，如果分数仍高则继续 gate。

**问题**：`checked <= issued` 条件仍然会导致 gate 后 while 退出，然后外层 for 继续下一个 warp。这实际上与论文语义一致——gate 后跳过该 warp，继续下一个。

### 方案 B2：在 `order_warps()` 后过滤候选列表

在 `order_warps()` 之后，从 `m_next_cycle_prioritized_warps` 中移除（或标记）高 LLS warp 的 load 候选。

**问题**：`m_next_cycle_prioritized_warps` 是 warp 级别的，不是 instruction 级别的。一个 warp 可能有 load 和 non-load 指令混合，不能简单移除整个 warp。

### 推荐：方案 B1（最小改动）

将 gate 从 `checkCollision()` 之后移到 `checkCollision()` 之前。改动量：约 10 行，只移动现有 gate 逻辑的位置。

**预期效果**：
- 高 LLS warp stall 恢复后，第一条 load 会被 gate（而不是直接发射）
- gate 后 `checked++`，while 退出，外层 for 继续下一个 warp
- 低 LLS warp 继续正常发射
- 非 load 指令（store/compute/control）不受影响

---

## 7. 三个方向的比较

### 方向 A：inc=50 + 更高 threshold

- 调整 `lg_score_threshold`（如 150/200）以减少 inc=50 的过度 gating
- **优点**：不改 src/，只改 config
- **缺点**：
  - 不解决根本问题（gate 位置错误）
  - threshold 调整是参数补救，不是机制修复
  - inc=50 的正反馈机制本质上是 gate 了错误的 warp（非 stall warp）
  - 与论文语义偏差更大

### 方向 B：移动 gate 到 checkCollision 之前（B1）

- 将 `ccws_lg_gate_load()` 调用从 scoreboard 通过后移到 scoreboard 检查前
- **优点**：
  - 更接近论文 Can Issue bit 语义
  - 最小改动（约 10 行移动）
  - 高 LLS warp stall 恢复后 load 会被 gate
  - 不改变 non-load 指令的行为
- **缺点**：
  - 需要修改 src/（但改动极小）
  - 仍然是 per-warp gate，不是 per-instruction gate（论文是 per-instruction）
  - 需要重新验证 feature_off 7/7 pass

### 方向 C：接受当前行为，进入 standard validation

- 用 inc=1/th=99（hotspot 5 blocks）+ inc=50（srad_v2/fdtd2d active gating）作为证明
- **优点**：不改 src/，快速进入 standard set
- **缺点**：
  - inc=50 的 gating 机制与论文不同（gate 了错误的 warp）
  - standard set 结果可能无法解释（cycle 增加而非减少）
  - 不是忠实复现

---

## 8. 推荐方向

**推荐方向 B（B1 方案）**。

理由：
1. 忠实复现优先于快速出信号
2. 改动极小（约 10 行，只移动现有代码）
3. 修复后 inc=5/20/30 预期会产生 gating 信号（高 LLS warp stall 恢复后被 gate）
4. 方向 A 是参数补救，不解决根本问题
5. 方向 C 的 inc=50 结果无法作为 CCWS 忠实复现的证据

---

## 9. Round AF 最小实现边界（如选择方向 B）

**改动范围**：`src/gpgpu-sim/shader.cc`，`scheduler_unit::cycle()` 内部，约 10 行移动。

**具体改动**：

```cpp
// 当前位置（scoreboard 通过后）：
if (!m_scoreboard->checkCollision(warp_id, pI)) {
  ready_inst = true;
  ...
  if ((pI->op == LOAD_OP) || ...) {
    // [Round Y gate 在这里]
    bool ccws_load_blocked = false;
    if (enable_ccws && enable_load_gating && is_load_op) {
      ccws_load_blocked = ccws_lg_gate_load(warp_id);
    }
    if (!ccws_load_blocked && m_mem_out->has_free(...)) {
      issue_warp(...);
    }
  }
}

// 目标位置（scoreboard 检查之前）：
// 在 if (pI) { assert(valid); if (pc != pI->pc) { ... } else { valid_inst = true;
//   [在这里插入 pre-scoreboard gate]
//   if (enable_ccws && enable_load_gating && is_load_op(pI) &&
//       ccws_lg_gate_load(warp_id)) {
//     checked++;  // 消耗一次 check slot
//     continue;   // 跳过此 warp 的此 instruction，但不 break
//   }
//   if (!m_scoreboard->checkCollision(...)) { ... }
```

**注意**：`continue` 在 while 循环中会重新检查条件 `checked <= issued`。由于 `issued=0`，`checked(1) <= issued(0)` → false → while 退出。效果与当前 gate 相同（跳过该 warp），但触发时机更早。

**验证要求**：
- feature_off 7/7 pass（cycle = baseline）
- load_gate_on：inc=5/20/30 至少一个 workload 出现 lg_block > 0
- STORE/compute/control 不受影响
- sim_cycle 变化 < 10%（目标）

**不需要改动**：
- `ccws_lg_gate_load()` 实现不变
- `would_can_issue` 计算不变
- LLS/VTA 逻辑不变
- 所有 config knob 不变
