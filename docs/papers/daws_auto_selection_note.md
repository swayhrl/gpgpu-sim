# DAWS 论文选择说明

**Round**: AUTO-1
**Date**: 2026-05-01

## 为什么选 DAWS 作为第二篇自动化试跑

1. **同属 warp scheduling 方向**：DAWS（Divergence-Aware Warp Scheduling，MICRO 2011）和 CCWS（Cache-Conscious Wavefront Scheduling，MICRO 2012）都在 `scheduler_unit::cycle()` 层面工作，复用 CCWS 复现中积累的 `shader.cc` / `shader.h` 经验。

2. **关注点不同，互补**：CCWS 关注 cache lost locality（LLS score → load gating），DAWS 关注 branch divergence（diverged warp → reconvergence priority）。两者机制链路不重叠，不会产生干扰。

3. **机制层次清晰**：DAWS 核心思路是：当 warp 发生分支分歧时，优先调度"接近 reconverge"的 warp，减少分歧执行周期。这与 CCWS 的 LLS score 排序结构类似，便于套用 Stage 02–04 的 telemetry → would-change → minimal mechanism 流程。

4. **GPGPU-Sim 已有 divergence 基础设施**：`simt_stack`、`warp_inst_t::active_count()`、`shader_core_ctx` 中的 warp 状态均可用于 divergence metric 探针，无需从零建立。

5. **近似实现可接受**：DAWS 的精确 divergence metric 可能需要近似，与 CCWS 的 VTA miss-side 近似情况类似。`do_not_require_100_percent_faithful` 规则已写入 `daws.yaml`。

## 本轮范围

本轮（AUTO-1）只建立 `tools/paper_repro/papers/daws.yaml`，不进入机制实现。
- 不读论文正文
- 不做源码 mapping
- 不创建 branch `hrl/paper/daws-repro-v0`
- 不创建 `experiments/paper-daws/`

下一轮 AUTO-2 才进入 `00_reading` stage，在新 branch 上开始阅读论文和源码 mapping。
