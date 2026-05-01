# CCWS Final Reproduction Report

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**论文**: Cache-Conscious Wavefront Scheduling (Rogers, O'Connor, Aamodt — MICRO 2012)  
**复现轮次**: Round S → Round AI（共 18 个 round）

---

## 1. Scope and Goal

本次复现目标是**机制趋势复现与流程打通**，不追求原论文 exact number。

具体目标：
1. 理解 CCWS 三层机制（VTA / LLS / Can-Issue gating）
2. 在 GPGPU-Sim 中实现完整机制链路
3. 验证 feature_off 行为不变（不破坏 baseline）
4. 验证 feature_on 时 gating 信号出现
5. 建立可复用的逐篇论文复现流程样板

**不追求**：原论文 benchmark suite 完整复现、exact IPC 数字、exact MPKI 数字。

---

## 2. Paper Mechanism Recap

CCWS 分三层：

**Layer 1 — VTA (Victim Tag Array) + LLD**  
当 warp W 的 cache line 被其他 warp 驱逐时，该 cache line 的 tag 写入 VTA[W]。W 下次 L1D miss 时 probe VTA[W]，若命中则触发 Lost Locality Detection（LLD）事件。

**Layer 2 — LLS (Lost-Locality Score)**  
每个 warp 维护 LLS。初始值 = BaseScore（100）。LLD 事件 → LLS 跳升至 LLDS（动态计算）。每周期 decay 回 BaseScore。LLDS 公式：
```
LLDS = (VTAHitsTotal / InstIssuedTotal) × K_THROTTLE × CumLLSCutoff
CumLLSCutoff = NumActiveWarps × BaseScore
```

**Layer 3 — Can-Issue gating**  
每周期对所有 warp 的 LLS 排序（max-heap），计算 prefix sum。LLS 落在 prefix sum 超过 CumLLSCutoff 之后的 warp，其 Can-Issue bit 清零，**只阻塞 load 指令**（non-load 不受影响）。

**HCS vs CI workload 预期**：
- HCS（High Cache Sensitivity）workload：L1D miss 率高，CCWS 减少 active warp 数，降低 cache thrashing，IPC 提升
- CI（Cache Insensitive）workload：L1D miss 率低，CCWS 不触发，IPC 不变

---

## 3. GPGPU-Sim Mapping

| 论文概念 | GPGPU-Sim 实现位置 | 文件 |
|---------|------------------|------|
| Wavefront Issue Arbiter | `scheduler_unit::cycle()` | `shader.cc` |
| VTA per-warp store | `m_ccws_vta[][]`（per-warp 环形 buffer）in `ldst_unit` | `shader.h/cc` |
| LLD event | `ccws_vta_probe_miss()` MISS branch | `shader.cc` |
| LLS score array | `m_ccws_lls[]` in `ldst_unit` | `shader.h/cc` |
| LLS decay | `ldst_unit::cycle()` 每 `lls_decay_interval` 周期 | `shader.cc` |
| Can-Issue sort+prefix-sum | `ldst_unit::cycle()` 末尾 | `shader.cc` |
| Can-Issue gate | `scheduler_unit::cycle()` pre-scoreboard（Round AF） | `shader.cc` |
| Load-only gating | `ccws_lg_gate_load(wid)` 查 `m_ccws_would_can_issue[wid]` | `shader.cc` |
| Config knobs | `shader_core_config`（21 个 `gpgpu_ccws_*` 成员） | `shader.h/gpu-sim.cc` |
| Stats counters | `paper_ccws_*`（15+ 个计数器） | `gpu-sim.cc` |

---

## 4. Implemented Stages

| Round | 内容 | 关键结果 |
|-------|------|---------|
| S | no-op config knobs + `paper_ccws_*` stats | 编译通过；feature_off 4/4 pass |
| T | no-op behavior check；`GPGPUSIM_CONFIG_OVERRIDE` 支持 | feature_off/on_noop 7/7 pass |
| U | SWL static baseline（limit_4/8/16 configs） | tiny workload 无法区分 SWL 效果 |
| V | VTA-like miss-side probe（`gpgpu_ccws_enable_vta_probe`） | vta_hit > 0；sim_cycle 不变 |
| W | LLS score array + per-cycle decay | lls_update = vta_hit（精确相等）；sim_cycle 不变 |
| X | Would-gate telemetry（sort+prefix-sum，不阻塞） | would_gate_block = 2 for hotspot；sim_cycle 不变 |
| Y | 真实 load-only gating（`enable_load_gating`） | hotspot lg_block=5；feature_off 7/7 pass |
| Z | Post-gating validation；base_score threshold sweep | 发现 base_score 不是独立 threshold |
| AA | 独立 `lg_score_threshold` knob | threshold sweep 有效；th<base_score deadlock |
| AB | Focused threshold validation（th99/100/101） | 趋势单调正确；信号弱 |
| AC | LLS hit-increment sensitivity（inc1/10/50） | inc=50 over-gating；inc=10 timing issue |
| AD | Hit-increment calibration（inc5/20/30） | 全 0 blocks；gate 只在 warp 有 load ready 时触发 |
| AE | Gate 插入点审计（docs only） | 确认 post-scoreboard 不是根本原因 |
| AF | Pre-scoreboard gate 实现 | inc=5/20/30 仍 0 blocks；VTA hits 分散是根本原因 |
| AG | Can-Issue cutoff 审计（docs only） | 确认 nw=max_warps=64 高估 8×；inactive warp 消耗 87.5% cutoff |
| AH | Active-warp cutoff 尝试 → revert | tiny workload over-gating（+64–767%）；接受近似实现 |
| AI | Focused validation（conservative/aggressive） | conservative +2–11% cycle；aggressive +390–1464% |

---

## 5. What Was Successfully Reproduced

| 机制 | 状态 | 证据 |
|------|------|------|
| feature_off 行为不变 | ✓ 完全复现 | 所有 round feature_off 7/7 pass；sim_cycle = baseline |
| VTA-like signal | ✓ 功能正确 | vta_hit > 0 for L1D-miss workloads；strided/page_stride 极低（符合预期） |
| LLS update 与 VTA hit 对齐 | ✓ 精确 | lls_update = vta_hit（所有 workload 精确相等） |
| Would-gate telemetry | ✓ 有信号 | would_gate_block > 0 for hotspot |
| Load-only gating | ✓ 真实阻塞 | lg_block > 0；STORE/compute 不受影响 |
| Conservative gating 信号 | ✓ 有信号 | inc=1, th=100：hotspot/srad_v2/fdtd2d/mutual_tiled/bfs 均有 lg_block |
| Aggressive gating 证明 | ✓ 过度 | inc=50：严重 over-gating，证明 gating 机制 active |
| Workload 区分 | ✓ 方向正确 | L1D-miss workload 有 gating；strided/page_stride 无 gating |

---

## 6. Approximation and Deviations

### 6.1 VTA：miss-side 近似（非 eviction-based）

**论文 VTA**：当 warp W 的 cache line 被其他 warp 驱逐时，记录到 VTA[W]。检测的是 **inter-warp eviction**。

**当前实现**：每次 L1D MISS 时，probe VTA[wid] 是否有相同 block_addr。若有，则 vta_hit（代表 warp 重复 miss 同一 block）。检测的是 **intra-warp repeated miss**。

**原因**：`evicted_block_info`（`gpu-cache.h:82`）无 warp_id 字段，faithful eviction-based VTA 需要修改 `cache_block_t`，风险较高。

**影响**：vta_hit 信号方向正确（L1D-miss workload 高，stride workload 低），但语义不同。

### 6.2 Can-Issue cutoff：max_warps 近似（非 active_warps）

**论文**：`CumLLSCutoff = NumActiveWarps × BaseScore`，NumActiveWarps 是当前 SM 上 active 的 warp 数。

**当前实现**：`nw = max_warps_per_shader = 64`，`cum_cutoff = 64 × lg_score_threshold = 6400`。

**原因**：用 `not_completed/warp_size` 替换时，tiny workload（srad_v2 64×64 → ~2 warps/SM）使 cutoff 降到 200，导致 91% loads 被阻塞（+64–767% cycle）。已 revert。

**影响**：cutoff 高估 8×。inactive warp 的 base_score 合计（56×100=5600）消耗 87.5% cutoff budget。gate 触发时机偏晚，conservative config 下 cycle 增加而非减少。

### 6.3 LLDS 计算：静态 increment 替代动态 LLDS

**论文**：LLDS 动态计算（VTAHitsTotal / InstIssuedTotal × K_THROTTLE × CumLLSCutoff）。

**当前实现**：VTA hit → LLS += `lls_hit_increment`（静态常数）。

**影响**：无法自适应调整 gating 强度。inc=1 信号弱，inc=50 过度 gating，无中间稳定点。

### 6.4 Workload：tiny focused set，非原论文 HCS/CI benchmark

原论文使用完整 Rodinia / SHOC / Parboil benchmark suite，workload 有足够 occupancy（16–32 warps/SM）。当前 tiny workload（64×64）每 SM 只有 2–8 warps，不代表论文 HCS 场景。

### 6.5 结论

**当前实现不能宣称 exact reproduction**。机制链路正确，但定量结果（cycle delta 方向、gating 强度）与原论文不符。

---

## 7. Key Experimental Findings

| Round | 关键发现 |
|-------|---------|
| V | vta_hit > 0 for L1D-miss workloads；atomic_contention vta_hit=0（正确） |
| W | lls_update = vta_hit（精确相等）；mutual_tiled 末尾 nonzero_warps=0（decay 平衡） |
| X/Y | would_gate_block=2 for hotspot；lg_block=5 for hotspot（真实 gating） |
| Z | base_score 不是独立 threshold（同时控制初始值和 cutoff） |
| AA | lg_score_threshold 独立 knob 有效；th < base_score → deadlock |
| AB | threshold 趋势单调正确（th99→5 blocks，th101→0 blocks） |
| AC/AD | inc=50 over-gating；inc=5/20/30 全 0 blocks（gate 只在 warp 有 load ready 时触发） |
| AE/AF | Pre-scoreboard gate 不改变结果；VTA hits 分散是根本原因 |
| AG/AH | nw=max_warps 高估 8×；active-warp fix 在 tiny workload 过度 gating |
| AI | conservative（inc=1, th=100）：5/7 workload 有 gating，cycle +2–11%；aggressive（inc=50）：+390–1464% |

**th=99 deadlock**：threshold < lls_base_score=100 导致所有 warp 在初始化时 LLS=100 < cutoff/nw=99，全部被 gate。有效范围：`lg_score_threshold >= lls_base_score`。

---

## 8. Interpretation

**为什么 conservative config cycle 增加而非减少？**

1. cutoff 高估 8×（nw=64 vs active ~8）→ gate 触发时机偏晚
2. gate 触发时 warp 的 LLS 已经很高，说明该 warp 已经造成了 cache thrashing
3. 此时 gate 阻塞 load → warp stall 时间增加 → cycle 增加
4. 正确的 CCWS 应在 thrashing 发生前就 gate（通过准确的 active-warp cutoff）

**为什么 strided_access / page_stride_access 无 gating？**

这两个 workload 的 stride 破坏了 intra-warp 局部性，每个 warp 的 miss 都是不同 block，VTA probe 几乎不命中（vta_hit=24）。LLS 分数几乎不超过 base_score，gate 不触发。这与预期一致——这两个 workload 不是 HCS workload。

**当前实现更适合证明机制链路，而不是性能收益。**

---

## 9. Reproduction Status

| 维度 | 状态 | 说明 |
|------|------|------|
| 论文机制理解 | ✓ 完成 | 三层机制（VTA/LLS/Can-Issue）全部理解并映射到 GPGPU-Sim |
| 机制链路实现 | ✓ 完成 | VTA→LLS→would-gate→load-gate 完整链路 |
| feature_off 不破坏 baseline | ✓ 完成 | 所有 round 7/7 pass |
| Faithful VTA | ✗ 未实现 | miss-side 近似；eviction-based 需修改 cache_block_t |
| Faithful cutoff | ✗ 未实现 | max_warps 近似；active-warp fix 在 tiny workload 过度 gating |
| 动态 LLDS 计算 | ✗ 未实现 | 静态 increment 替代 |
| Gating 信号出现 | ✓ 完成 | conservative config 5/7 workload 有 lg_block > 0 |
| Cycle 方向正确 | ✗ 未达到 | conservative +2–11%（应减少）；根本原因：cutoff 近似 |
| 论文 benchmark 复现 | ✗ 未做 | tiny focused workload，非原论文 HCS/CI suite |
| 复现流程样板 | ✓ 完成 | 18 round 流程可复用；config matrix / round_state / validation CSV 体系建立 |

**总结**：机制链路复现成功，faithful reproduction 部分完成，定量结果与原论文不符。作为第一篇论文复现流程样板，目标达成。

---

## 10. Recommended Next Steps

### 不建议在当前 paper branch 继续

当前 `hrl/paper/ccws-repro-v0` 已完成其使命。不建议继续在此分支调机制。

### 建议进入 FINAL-INFRA

将 CCWS 复现流程沉淀为自动化逐篇复现框架：
- 标准化 round_state.yaml schema
- 自动化 feature_off / feature_on 验证脚本
- config matrix 自动生成
- 结果 CSV 自动汇总

### 后续如需 faithful CCWS（可选，另开 extended round）

1. **Faithful eviction-based VTA**：在 `cache_block_t` 加 `m_warp_id` 字段；在 eviction 时写入 VTA[warp_id]
2. **Active-wavefront-aware cutoff**：需要更大 workload（srad_v2 256×256 等）才能避免 tiny workload over-gating
3. **动态 LLDS 计算**：实现 VTAHitsTotal / InstIssuedTotal 比率计算

### 自研 cache policy

不混入 paper branch。另开 `hrl/idea/cache-policy-experiments-v0`，基于当前机制链路做自研改进。
