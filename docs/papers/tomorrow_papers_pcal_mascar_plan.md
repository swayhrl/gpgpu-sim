# Tomorrow Papers Plan — PCAL & Mascar

**Date prepared**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0（当前，明天各自开新 branch）

---

## 为什么选 PCAL

**PCAL（Priority-Based Cache Allocation, HPCA 2015）**

- 与 cache policy 自研方向直接相关：PCAL 的核心是在 GPU 上对线程分优先级做 cache 分配/bypass。
- 与 CCWS 的关系：CCWS 通过 warp scheduling 间接减少 cache 压力；PCAL 直接控制哪些 warp 的数据被 cache。两者是互补思路。
- 与 cache policy 自研的关系：PCAL 的 bypass/admission 机制是自研 cache policy 的直接前置参考。复现后可以把 PCAL 的 bypass 信号和 CCWS 的 warp 优先级信号结合。
- 风险：需要深入 `gpu-cache.cc`，比 CCWS/DAWS 更靠近 cache 硬件层。第一遍用 miss-rate 代理信号，不直接改 cache replacement。

---

## 为什么选 Mascar

**Mascar（Memory Pitstop Scheduling, HPCA 2015）**

- 与 DAWS 的关系：DAWS 关注线程分叉（divergence）引起的 warp 低效；Mascar 关注内存饱和（memory saturation）引起的 warp 阻塞。两者信号来源不同，但都属于"识别低效 warp 并调度绕开"的大框架。
- 与 memory-aware scheduling 自研的关系：Mascar 的 MSHR 饱和探测是内存感知调度的基础模块。
- 风险：memory pipeline 改动比 scheduler 改动风险更高。第一遍**只做被动探测**，不实现 pipeline replay。
- 特别约束：**不实现 re-execution/replay**，这是 Mascar 的核心 speedup 来源，但在 GPGPU-Sim 中实现风险极高。

---

## 明天建议先跑哪些 stage

| 论文 | Stage | 可无人值守 | 原因 |
|------|-------|-----------|------|
| PCAL | `00_reading` | ✓ | 无代码改动 |
| PCAL | `01_noop` | ✓ | 只加 config，feature_off 不变 |
| PCAL | `02_telemetry` | 执行后 stop | 被动探测，完成后需 review |
| Mascar | `00_reading` | ✓ | 无代码改动 |
| Mascar | `01_noop` | 执行后 stop | memory pipeline 风险，noop 完成后要确认再推进 |

**不放入 tomorrow queue 的 stage**：

- `04_minimal_mechanism`（两篇）：第一次行为改动，永远 blocked
- `06_standard_validation`（两篇）：需要 cycle 方向分析
- PCAL cache replacement 改动：需要单独 review
- Mascar pipeline replay：第一遍完全不做

---

## 哪些 stage 必须 stop_for_review

| Stage | 原因 |
|-------|------|
| `04_minimal_mechanism` | supervisor 永远 `blocked_high_risk_stage` |
| PCAL `02_telemetry`（完成后）| `stop_after_completion=true` |
| Mascar `01_noop`（完成后）| `stop_after_completion=true`，需确认 memory 模块没有意外改动 |
| 任何 src/ 意外改动 | `stop_on_src_diff=true` 触发 |
| feature_off 后 sim_cycle 改变 | 基线破坏，立即停 |

---

## 为什么不追求 100% faithful reproduction

1. **工具不可达**：PCAL/Mascar 原始 benchmark 套件（SPEC CPU, Parboil 全集）未全安装。
2. **硬件细节未知**：exact cache set associativity、MSHR 参数可能与原论文不同。
3. **目标是信号验证**：我们需要确认"机制链路可工作"（信号 → 判断 → 动作），而不是精确复现数字。
4. **前置知识积累**：approximate reproduction 给 cache policy 自研提供足够的代码基础和 config 体系。

---

## 论文优先级和执行顺序

明天建议按顺序：PCAL reading → PCAL noop → PCAL telemetry（stop for review）→ Mascar reading → Mascar noop（stop for review）。

两篇都完成 noop 之后，回来评估是否继续 telemetry/would-change，再决定 minimal_mechanism 是否做。
