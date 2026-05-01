# PCAL Phase 1：No-op Config + Stats

**日期**：2026-05-01  
**分支**：hrl/paper/pcal-repro-v0  
**状态**：完成 ✓

---

## 目标

添加 PCAL 默认关闭的 config knobs 和 paper_pcal_* stats 占位，不改变任何行为。

## 修改文件

| 文件 | 改动 |
|------|------|
| `src/gpgpu-sim/shader.h` | `shader_core_config` 中添加 7 个 PCAL knobs；`shader_core_ctx` 中添加 8 个成员变量 + `get_pcal_stats()` 方法；`simt_core_cluster` 中添加 `get_pcal_stats()` 声明 |
| `src/gpgpu-sim/shader.cc` | 构造函数中初始化 8 个 PCAL 成员变量；添加 `simt_core_cluster::get_pcal_stats()` 实现 |
| `src/gpgpu-sim/gpu-sim.cc` | `reg_options()` 中注册 7 个 PCAL knobs；`gpu_print_stat()` 中添加 paper_pcal_* 输出 |
| `configs/hrl-repro/SM7_QV100_pcal_noop_off/` | 新建：master off |
| `configs/hrl-repro/SM7_QV100_pcal_noop_on/` | 新建：master on，所有 stages off |

## 添加的 Config Knobs

| Knob | 默认值 | 用途 |
|------|--------|------|
| `gpgpu_enable_pcal` | 0 | master switch |
| `gpgpu_pcal_enable_telemetry` | 0 | Phase 2: cache pressure probe |
| `gpgpu_pcal_enable_would_bypass` | 0 | Phase 3: would-bypass telemetry |
| `gpgpu_pcal_enable_bypass` | 0 | Phase 4: actual L1D bypass |
| `gpgpu_pcal_miss_rate_threshold` | 50 | miss rate % 分类阈值 |
| `gpgpu_pcal_window_size` | 64 | per-warp sliding window size |
| `gpgpu_pcal_debug` | 0 | per-cycle debug trace |

## 添加的 Stats

| Stat | 说明 |
|------|------|
| `paper_pcal_enabled` | master switch 状态 |
| `paper_pcal_miss_event` | per-warp miss 事件计数 |
| `paper_pcal_access_event` | per-warp access 事件计数 |
| `paper_pcal_warp_classified_high` | 高 priority warp 分类次数 |
| `paper_pcal_warp_classified_low` | 低 priority warp 分类次数 |
| `paper_pcal_would_bypass` | would-bypass 判断次数 |
| `paper_pcal_bypass_count` | 真实 bypass 次数 |
| `paper_pcal_bypass_hit` | bypass 后原本为 hit 的次数 |
| `paper_pcal_window_reset` | window 重置次数 |

## 验证结果

| Workload | Config | sim_cycle | paper_pcal_enabled | behavior stats |
|----------|--------|-----------|-------------------|----------------|
| vecadd | noop_off | 5569 | 0 | 全部 0 |
| vecadd | noop_on | 5569 | 1 | 全部 0 |
| rodinia_hotspot | noop_off | 6931 | 0 | 全部 0 |
| rodinia_hotspot | noop_on | 6931 | 1 | 全部 0 |
| rodinia_srad_v2 (K1) | noop_off | 8236 | 0 | 全部 0 |
| rodinia_srad_v2 (K1) | noop_on | 8236 | 1 | 全部 0 |

- feature_off sim_cycle = baseline ✓
- feature_on_noop sim_cycle = feature_off ✓  
- 除 paper_pcal_enabled 外，所有行为计数为 0 ✓
- 无真实 PCAL 行为 ✓

## 成功标准检查

- [x] 编译通过
- [x] feature_off cycle 与 baseline 一致
- [x] feature_on_noop cycle 与 feature_off 一致
- [x] 除 enabled 状态外，paper_pcal_* 行为计数为 0
- [x] 没有真实 PCAL 行为改动
