# PCAL Reproduction Plan

**Round**: Phase 2 (cache_pressure_telemetry) 完成
**Date**: 2026-05-01
**状态**: Phase 2 完成 ✓，进入 Phase 3 would-change

---

## 1. 论文机制链

```
[Miss Rate Probe]
    每次 L1D access：
        access_count[warp_id]++
        如果 MISS：miss_count[warp_id]++
        如果超过 window_size：重置并更新 miss_rate[warp_id]
        ↓
[Priority Classification]
    miss_rate[warp_id] > pcal_miss_rate_threshold
        → 低优先级（cache-insensitive）→ bypass
    miss_rate[warp_id] ≤ threshold
        → 高优先级（cache-sensitive）→ 正常缓存
        ↓
[L1 Bypass Gate]
    shader.cc:2463 处加入：
    if (enable_bypass && pcal_is_low_priority(warp_id)):
        bypassL1D = true
```

---

## 2. 阶段规划

| 阶段 | 内容 | src 改动 | 预计 round |
|------|------|---------|-----------|
| 00_reading | 本阶段，阅读 + 映射 | 无 | AUTO-9 |
| 01_noop | config 骨架 + 6 knob + 11 stat + feature_off 验证 | shader.h, shader.cc, gpu-sim.cc | AUTO-10 |
| 02_telemetry | per-warp miss/access 计数 + miss_rate 计算 | shader.h, shader.cc | AUTO-11 |
| 03_would_change | would-bypass 遥测（passive，不改缓存行为） | shader.cc | AUTO-12 |
| 04_minimal_mechanism | 真实 L1 bypass gate | shader.cc | AUTO-13 |
| 05_focused_validation | 7 workloads × 3 configs | 无 | AUTO-14 |
| 07_final_report | 最终报告 | 无 | AUTO-15 |

---

## 3. Config Knobs 规划（Stage 01）

```ini
# 主开关（默认 0）
gpgpu_enable_pcal                  0

# Stage 02: 遥测开关
gpgpu_pcal_enable_telemetry        0

# Stage 03: would-bypass 遥测
gpgpu_pcal_enable_would_bypass     0

# Stage 04: 真实 bypass
gpgpu_pcal_enable_bypass           0

# 参数
gpgpu_pcal_miss_rate_threshold     50   # 0-100，超过此值 → bypass
gpgpu_pcal_window_size             64   # per-warp access window 大小
```

---

## 4. Stats 规划（Stage 01）

```
paper_pcal_miss_event             # L1D miss 被 probe 的次数
paper_pcal_access_event           # L1D access 被 probe 的次数
paper_pcal_warp_classified_high   # 被分类为 high priority 的 warp-round 数
paper_pcal_warp_classified_low    # 被分类为 low priority 的 warp-round 数
paper_pcal_would_bypass           # 如果 bypass 开启，应该 bypass 的 access 数（telemetry）
paper_pcal_bypass_count           # 实际 bypass 的 access 数
paper_pcal_bypass_hit             # 被 bypass 的 access 中原本会命中的（用于评估 bypass 是否激进）
paper_pcal_window_reset           # 窗口重置次数
paper_pcal_all_low_priority       # 所有 warp 同时低优先级的周期数（死锁风险指示）
paper_pcal_high_priority_warps    # 平均高优先级 warp 数
paper_pcal_low_priority_warps     # 平均低优先级 warp 数
```

---

## 5. 关键实现约束

### 必须保证

1. `gpgpu_enable_pcal=0` → `sim_cycle` = baseline（5569），所有 `paper_pcal_*` = 0
2. bypass 只对 global memory load（`inst.space.is_global() && inst.is_load()`）
3. per-warp 数组大小 = `m_config->max_warps_per_shader`（与 DAWS m_daws_footprint 相同 pattern）
4. 窗口重置以 access 数为准（非 cycle 数），避免 idle warp 分类卡死

### 已知风险与对策

| 风险 | 对策 |
|------|------|
| 所有 warp bypass → 无数据返回 | miss_rate_threshold 默认 50（保守）；可配置 |
| 死锁（bypass ICNT 满） | GPGPU-Sim bypassL1D 路径自带 back-pressure（ICNT_RC_FAIL stall），不死锁 |
| HIT_RESERVED 情况下 bypass 判断 | bypass 在 accessq 阶段判断（pre-issue），HIT_RESERVED 后的重发不重新 bypass |

---

## 6. 近似说明（相比原论文）

| 原论文（推测） | 本复现 |
|-------------|-------|
| 可能 per-CTA 分类 | per-warp 分类 |
| 可能有复杂 CSI 指标 | 简单 miss/access rate |
| 可能 L2 也 bypass | 只 bypass L1D（L2 bypass 需更大改动）|
| 可能有 priority 队列 | 只做 binary high/low 分类 |
| Adaptive update 可能更复杂 | 固定 window size 重置 |

---

## 7. 目标 Workload Set

**Focused set（7 workloads）**：
```
rodinia_hotspot, rodinia_srad_v2, polybench_fdtd2d,
mutual_tiled, polybench_2dconv, strided_access, parboil_histo
```

**Control set（5 workloads，应该几乎不触发 bypass）**：
```
vecadd, polybench_gemm, mutual_naive, rodinia_backprop, atomic_contention
```

---

## 8. 成功标准

| 阶段 | 成功标准 |
|------|---------|
| 01_noop | feature_off sim_cycle = 5569；所有 paper_pcal_* = 0 |
| 02_telemetry | focused workloads 有 miss_event > 0；sim_cycle 不变 |
| 03_would_change | focused workloads 有 would_bypass > 0；sim_cycle 不变 |
| 04_minimal_mechanism | bypass_count > 0；无死锁；feature_off 不变 |
| 05_validation | 全程无 deadlock；feature_off ≈ baseline；bypass workloads cycle delta 有方向 |
