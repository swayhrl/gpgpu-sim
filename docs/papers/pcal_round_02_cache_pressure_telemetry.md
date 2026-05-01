# PCAL Phase 2：Cache Pressure Telemetry

**日期**：2026-05-01  
**分支**：hrl/paper/pcal-repro-v0  
**状态**：完成 ✓

---

## 目标

实现 passive cache pressure telemetry，观测 per-warp miss rate 信号，不改变 cache 行为。

## 修改文件

| 文件 | 改动 |
|------|------|
| `src/gpgpu-sim/shader.h` | `shader_core_ctx` 添加 3 个 per-warp 向量（accesses/misses/low_priority）和 `pcal_probe_access()` 内联方法 |
| `src/gpgpu-sim/shader.cc` | 构造函数初始化 3 个新向量；`process_cache_access()` HIT/MISS 路径添加 probe；l1_latency queue 路径添加 probe（含 use-after-free 修复） |
| `configs/hrl-repro/SM7_QV100_pcal_telemetry_on/` | 新建：telemetry on，window_size=8（小 workload 适配） |

## 实现机制

`pcal_probe_access(warp_id, is_miss)` 每次 L1D 访问时调用：
1. 递增 per-warp access 和 miss 计数
2. 当 window 满时（`accesses[warp_id] >= window_size`），计算 miss_pct
3. `miss_pct >= miss_rate_threshold` → 分类为 low priority；否则 high priority
4. 重置 window 计数器

Hook 点：两条 L1D 访问路径（process_cache_access + l1_latency queue），各 HIT 和 MISS 分支。

## 近似说明

- 使用 miss/access 比率作为 priority proxy（非 PCAL 原论文的 CSI metric）
- per-warp 分类（非 per-CTA）
- binary priority（非 multi-level）
- window_size=8（原论文更大）；tiny workload 必须减小

## 验证结果

| Workload | Config | sim_cycle | access_event | miss_event | miss_rate | classified_high | classified_low | window_reset |
|----------|--------|-----------|-------------|------------|-----------|----------------|----------------|-------------|
| vecadd | feature_off | 5569 | 0 | 0 | — | 0 | 0 | 0 |
| vecadd | telemetry_on | 5569 | 96 | 96 | 100% | 0 | 8 | 8 |
| rodinia_hotspot | feature_off | 6931 | 0 | 0 | — | 0 | 0 | 0 |
| rodinia_hotspot | telemetry_on | 6931 | 2576 | 2576 | 100% | 0 | 262 | 262 |
| rodinia_srad_v2 K1 | feature_off | 8236 | 0 | 0 | — | 0 | 0 | 0 |
| rodinia_srad_v2 K1 | telemetry_on | 8236 | 4352 | 4208 | 97% | 0 | 512 | 512 |
| polybench_fdtd2d | feature_off | 5840/...35681 | 0 | 0 | — | 0 | 0 | 0 |
| polybench_fdtd2d | telemetry_on | 5840/...35681 | 14450 | 10656 | 74% | 0 | 1772 | 1772 |

**信号解释**：
- 所有 workload miss_rate > 50% → 全部 classified_low，classified_high=0
- polybench_fdtd2d miss_rate~74% 有真实 cache reuse（数据重用）
- policy counters（would_bypass/bypass_count）全部 = 0 ✓
- sim_cycle 全部不变 ✓

## 成功标准检查

- [x] 编译通过
- [x] feature_off cycle 不变
- [x] telemetry_on cycle 不变
- [x] telemetry signal 合理且可解释
- [x] 无真实 policy 行为（policy counters = 0）
