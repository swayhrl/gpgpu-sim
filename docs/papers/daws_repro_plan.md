# DAWS Reproduction Plan

**Paper**: Divergence-Aware Warp Scheduling (Rogers, O'Connor, Aamodt — MICRO 2013)
**Branch**: hrl/paper/daws-repro-v0
**Round**: AUTO-8 (final report complete)
**Date**: 2026-05-01

---

## 复现目标

实现 DAWS 的近似版本，验证机制趋势（divergent workload 下 cycle 减少），不要求 100% faithful reproduction。

---

## Stage 序列

| Stage | 内容 | src 改动 | 预计 Round |
|-------|------|---------|-----------|
| 00_reading | 论文阅读 + GPGPU-Sim mapping | 无 | AUTO-2 ✓ |
| 01_noop | 添加 config knobs，feature_off 不改行为 | shader.h/cc, gpu-sim.cc | AUTO-3 ✓ |
| 02_telemetry | active_count probe + divergence event counter | shader.h/cc | AUTO-4 ✓ |
| 03_would_change | would-throttle telemetry（不实际 gate） | shader.h/cc | AUTO-5 ✓ |
| 04_minimal_mechanism | 实际 warp throttling（approximate footprint） | shader.h/cc | AUTO-6 ✓ |
| 05_focused_validation | 7 divergence-heavy workloads × 3 configs | 无 | AUTO-7 ✓ |
| 07_final_report | 复现报告 | 无 | AUTO-8 ✓ |

---

## 机制链路（近似版）

```
active_count() < warp_size
    → divergence_detected[wid] = true
    → predicted_footprint[wid] = f(warp_size, active_count)
    → sum(footprint) > L1_capacity_threshold
        → throttle: block warp from issuing
```

---

## GPGPU-Sim Mapping

| DAWS 概念 | GPGPU-Sim 位置 | 文件 |
|-----------|---------------|------|
| active thread count | `warp_inst_t::active_count()` / `m_warp_active_mask.count()` | abstract_hardware_model.h:1191 |
| divergence event | `simt_stack` pdom reconvergence | abstract_hardware_model.h:439 |
| warp state | `shd_warp_t` | shader.h |
| scheduler hook | `scheduler_unit::cycle()` | shader.cc |
| L1 cache config | `gpgpu_cache_config` | gpu-sim.cc |
| per-warp score array | 新增 `m_daws_footprint[]`（类似 `m_ccws_lls_score[]`） | shader.h |
| throttle gate | 新增 `m_daws_would_throttle[]`（类似 `m_ccws_would_can_issue[]`） | shader.h |

---

## Config Knobs 规划

```
gpgpu_enable_daws                    0    # master switch
gpgpu_daws_enable_telemetry          0    # active_count probe
gpgpu_daws_enable_would_throttle     0    # would-throttle telemetry
gpgpu_daws_enable_throttling         0    # actual warp throttling
gpgpu_daws_footprint_threshold       0    # L1 capacity fraction (0-100%)
gpgpu_daws_min_active_threads        1    # minimum active threads to trigger
```

---

## Stats 规划

```
paper_daws_divergence_event          # active_count < warp_size 事件数
paper_daws_footprint_update          # footprint 更新次数
paper_daws_would_throttle_block      # would-throttle 阻止次数
paper_daws_throttle_block            # 实际 throttle 阻止次数
```

---

## 近似说明

| 近似 | 原论文 | 近似实现 | 影响 |
|------|--------|---------|------|
| loop detection | 只在 loop 内预测 footprint | 全程检测 active_count | 可能误触发，但方向正确 |
| load classification | per-static-PC 分类表 | 所有 load 均视为 divergence-sensitive | 过度 throttle 风险 |
| footprint formula | 基于 loop iteration + active count | `(warp_size - active_count) * cache_line_size` | 近似，趋势应正确 |

---

## 最高风险点

1. **Loop detection 缺失**：近似方案可能在非 loop 区域误触发 throttle → cycle 增加。
2. **Tiny workload 低 divergence**：64×64 workload 可能 active_count 始终 = warp_size → 机制不触发。
3. **Throttle 方向**：DAWS throttle 高 footprint warp（低 active_count），与 CCWS gate 高 LLS score 方向不同，需要仔细验证。

---

## 成功标准

1. feature_off：sim_cycle = baseline，所有 `paper_daws_*` = 0。
2. telemetry：divergence-heavy workload（BFS、hotspot）有 `paper_daws_divergence_event > 0`。
3. would-throttle：至少 1 个 workload 有 `paper_daws_would_throttle_block > 0`。
4. throttling：divergence-heavy workload cycle 减少（或至少不大幅增加）。
5. 不要求 cycle 方向 100% 正确（CCWS 经验：cutoff 近似可能导致方向相反）。

---

## 与 CCWS 复现的差异

- CCWS 从 cache miss 侧入手（VTA probe in ldst_unit）；DAWS 从 scheduler 侧入手（active_count in scheduler_unit）。
- DAWS 的 telemetry hook 更靠近 scheduler，不需要修改 ldst_unit。
- DAWS 的 throttle 逻辑比 CCWS 的 load-only gating 更简单（不需要区分 LOAD_OP）。
- DAWS 的近似风险主要在 loop detection，CCWS 的近似风险主要在 VTA eviction-based 检测。
