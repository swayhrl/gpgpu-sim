# Mascar Round 03：Would-Change Scheduling Telemetry

**阶段**：Phase 3
**日期**：2026-05-01
**分支**：hrl/paper/mascar-repro-v0

---

## 目标

基于 Phase 2 stall_streak 信号，计算 would_deprioritize decision。
仍不改变 scheduler 行为。

---

## 改动文件

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| src/gpgpu-sim/shader.h | 修改 | 添加 mascar_check_would_deprioritize 方法 |
| src/gpgpu-sim/shader.cc | 修改 | 在 mem stall else 分支调用 would_deprioritize check |
| configs/hrl-repro/SM7_QV100_mascar_would_change_on/ | 新建 | would_change_on config |

---

## Would-Deprioritize 逻辑

触发条件：`stall_streak[warp_id] >= 2`
（即该 warp 已连续 2+ 次无法 issue 内存指令）

比 Phase 4 的 stall_threshold=8 更敏感，用于确认信号存在。

---

## 验证结果

| Workload | Config | sim_cycle | mem_stall | would_depr | skip | PASS |
|----------|--------|-----------|-----------|------------|------|------|
| vecadd | noop_off | 5569 | 0 | 0 | 0 | PASS (regression) |
| hotspot | noop_off | 6931 | 0 | 0 | 0 | PASS (regression) |
| vecadd | would_change_on | 5569 | 57 | 44 | 0 | PASS |
| hotspot | would_change_on | 6931 | 12150 | 10025 | 0 | PASS |
| srad_v2 | would_change_on | 8236 | 3989 | 3237 | 0 | PASS |
| bfs | would_change_on | 6665 | 1 | 0 | 0 | PASS (near-zero, expected) |
| strided_access | would_change_on | 5825 | 57 | 44 | 0 | PASS |
| fdtd2d | would_change_on | 5840 | 2400 | 1936 | 0 | PASS |

---

## 信号解读

- `would_deprioritize / mem_stall_event` 比率 ≈ 82%（hotspot：10025/12150）
  说明约 82% 的 stall 事件发生在 streak >= 2 时，即 warp 已在连续 stall 状态
- bfs would_deprioritize=0 符合预期（mem pressure 极低）
- 所有 sim_cycle 不变：would-change 为纯被动 telemetry ✓
- skip_count 全部为 0：无真实 policy 动作 ✓

---

## Phase 4 可行性判断

- 信号充分：hotspot/srad_v2/fdtd2d 均有明显 would_deprioritize 信号
- Hook 点确定：`scheduler_unit::cycle()` 中 `m_mem_out->has_free()` false 分支
- 计划修改文件：shader.h（1个方法）+ shader.cc（~10行调用）
- 不涉及：ldst_unit, MSHR, scoreboard, cache, DRAM, interconnect
- deadlock 防护：max_skip_streak counter（默认 4）
- 结论：**可以安全进入 Phase 4**

---

## 成功标准评估

- [x] would_deprioritize 有合理信号
- [x] 行为不变
- [x] 无真实 scheduling/memory policy 改动
- [x] 可以判断是否进入 minimal mechanism

Phase 3 通过，进入 Phase 4。
