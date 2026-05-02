# Mascar Reading Notes

**论文**：Mascar: Speeding up GPU Warps by Reducing Memory Pitstops
**作者**：Ankit Sethia, D. Anoushe Jamshidi, Scott Mahlke
**会议**：HPCA 2015
**阅读日期**：2026-05-01

---

## 1. 研究对象

GPU 上的 warp 调度效率问题，特别是 warp 在等待内存返回时浪费 pipeline 资源的场景。

---

## 2. 动机 / 问题

GPU 通常以 fine-grained multithreading 隐藏 memory latency：当一个 warp 等待内存返回时，调度另一个 warp 执行。这在 memory latency 适中时有效。

然而，当 **memory subsystem 本身饱和**（MSHR entries 被大量占用、memory queue 满载）时：

- 继续调度其他 memory-intensive warp 只会让 memory queue 更满；
- 已在内存中飞行的请求也难以得到服务，因为 queue 满导致 MSHR 无法 allocate；
- 结果是所有 warp 都陷入等待：warp 饥饿 + memory subsystem 过载，形成恶性循环。

**核心观察**：存在一类特殊的执行阶段，称为 "memory pitstop"，即某 warp 的所有（或绝大部分）执行都被 memory stall 阻塞，同时 memory subsystem 已经饱和。此时该 warp 继续竞争调度无益。

---

## 3. Mascar 核心方法

### 3.1 Memory Pitstop 检测

- **信号来源**：per-warp MSHR 占用程度（outstanding memory requests 数量）；
- **饱和判定**：当某 warp 的 outstanding requests 超过阈值（例如 MSHR > 50% 满），认为该 warp 处于 "memory pitstop"；
- **近似 (approximation)**：在 GPGPU-Sim 中，可用 per-warp memory stall 累积计数（warp 尝试 issue memory instruction 但被 ldst_unit 拒绝）作为代理信号。

### 3.2 Warp Deprioritization

- 当 warp 检测为 pitstop 状态时，将其在调度候选中降低优先级（跳过或延迟发射）；
- 让其他非饱和 warp 优先执行，以减少新 memory pressure；
- 同时允许已在飞行的 memory requests 有机会返回，从而排空 MSHR。

### 3.3 Re-execution / Pitstop Recovery（**本次 approximation 不实现**）

- 原论文可能涉及：pitstop warp 在内存返回后需要某种 re-launch 或 re-execution 机制；
- 这是 Mascar 相比简单 DAWS-style throttling 的主要区别；
- **本次实现不涉及 pipeline replay 或 re-execution**，这是已知限制。

### 3.4 调度恢复

- 当 warp 的 MSHR 占用降低（memory pressure 缓解），恢复其正常调度优先级；
- 需要 streak / cooldown counter 防止 deadlock（避免一个 warp 被永远跳过）。

---

## 4. 与 DAWS / CCWS / PCAL 的关系

| 论文 | 核心信号 | 调度动作 | 与 Mascar 区别 |
|------|---------|---------|--------------|
| CCWS | VTA probe（cache miss 导致的 warp 局部性下降）| 降低 LLS score 低的 warp 优先级 | CCWS 关注 cache 命中率；Mascar 关注 MSHR 饱和 |
| DAWS | Thread divergence（active_count < warp_size） | Throttle 发散 warp | DAWS 关注分叉效率；Mascar 关注内存饱和 |
| PCAL | Cache miss rate + warp priority | Per-warp cache bypass/admission | PCAL 改 cache 行为；Mascar 改 scheduling |
| Mascar | MSHR / memory queue saturation | Deprioritize saturated warp | 内存感知调度，不改 cache 替换 |

共同框架：识别"低效 warp" → 调整调度 → 让 memory/cache subsystem 有机会恢复。

---

## 5. 映射到 GPGPU-Sim 的模块

| Mascar 概念 | GPGPU-Sim 模块 | 近似实现 |
|-------------|---------------|---------|
| MSHR 占用监测 | `ldst_unit` / `baseline_cache::m_miss_queue` | per-warp stall streak counter |
| Warp 饱和判定 | `scheduler_unit` issue check | 读取 per-warp stall flag |
| 调度 deprioritize | `scheduler_unit::cycle()` issue-order | 跳过 pitstop warp，继续尝试下一个候选 |
| Pitstop 恢复 | streak counter <= max_skip_streak | 防 deadlock：max_skip_streak 次后强制允许 issue |
| Re-execution | **不实现** | 已知限制 |

**关键 hook 位置**：`scheduler_unit::cycle()` 中，在 candidate warp 被 issue 前，检查其 `m_mascar_stall_streak` 是否超过 `gpgpu_mascar_stall_threshold`；若超过且本 warp skip 次数 < `gpgpu_mascar_max_skip_streak`，跳过并继续下一个候选。

---

## 6. 可接受的 Approximation

| 原论文特性 | 本次 approximation | 影响 |
|----------|-------------------|------|
| MSHR entry count 精确监测 | 用 ldst issue fail count（per-warp）代替 | 信号粒度粗，可能 over-trigger |
| 精确 pitstop 进入/退出条件 | stall_streak > threshold 进入；streak=0 退出 | 简单但功能等价 |
| Re-execution / replay | 不实现 | 可能丢失主要 speedup 来源 |
| Per-CTA 或 per-warp 动态 threshold | 全局静态 threshold | 可能需要手动调参 |
| 原论文 baseline（可能不同 GPU arch） | GPGPU-Sim QV100（SM7）| 定量结果不可比 |

---

## 7. 高风险点

| 风险 | 说明 | 缓解措施 |
|------|------|---------|
| Deadlock | 若所有 warp 都被 skip，GPU 卡住 | max_skip_streak counter，超过后强制 issue |
| 过度 deprioritize | stall_streak 阈值过低 → 所有 memory-bound warp 被连续跳过 | 保守默认 threshold，focused validation |
| 非预期 cycle 方向 | tiny workload 可能无明显 memory pressure | 用 focused workloads（hotspot/srad_v2） |
| memory pipeline 重构 | 如果需要改 ldst_unit / MSHR / cache miss return | **强制停机**，不做 |
| re-execution 范围蔓延 | 任何 re-execution/replay 语义都是 HARD STOP | Phase 4 前再次确认 |

---

## 8. Phase 0 结论

**可行性判断**：Mascar approximate reproduction 在不实现 re-execution 的前提下，可以安全地：
1. 添加 per-warp memory stall telemetry；
2. 计算 would-deprioritize 信号；
3. 在 `scheduler_unit::cycle()` 实现最小 issue-order skip。

**不可行部分**：真实 pipeline replay / re-execution（已知限制，文档化即可）。

**建议**：继续推进 Phase 1 → 2 → 3 → 4（谨慎）。
