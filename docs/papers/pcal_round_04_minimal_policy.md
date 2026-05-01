# PCAL Round 04: Minimal Cache Allocation Policy

**Date**: 2026-05-01
**Branch**: hrl/paper/pcal-repro-v0
**Status**: Complete ✓

## 目标

实现 Phase 4 minimal PCAL bypass mechanism：将被分类为低优先级（high miss rate）的 warp 的 global load access bypass L1D cache。

## 实现

### 绕过机制（shader.cc memory_cycle()）

在 `memory_cycle()` 的 bypassL1D 判断链末尾添加 PCAL check：

```cpp
bool pcal_bypass = false;
if (!bypassL1D && m_core->pcal_is_warp_low_priority(inst.warp_id())) {
  bypassL1D = true;
  pcal_bypass = true;
  m_core->pcal_count_bypass();
}
```

创建 mf 时若 PCAL bypass 且 is_load()，插入 `m_pcal_bypass_mfs` set。

### 响应路径修复（shader.cc ldst_unit::cycle()）

在 response_fifo 处理路径中，添加 bypass set 检查，防止向 L1D 发出 fill 请求：

```cpp
bool pcal_bypassed = m_pcal_bypass_mfs.count(mf) > 0;
if (!bypassL1D && pcal_bypassed) {
  bypassL1D = true;
}
if (bypassL1D) {
  if (m_next_global == NULL) {
    ...
    if (pcal_bypassed) m_pcal_bypass_mfs.erase(mf);
    m_next_global = mf;
  }
}
```

**关键 bug 修复**：erase 只在成功 pop 时执行（`m_next_global == NULL`），否则 mf 留在 fifo 中下一 cycle 再处理。若提前 erase 则 set 中找不到 → 触发 `baseline_cache::fill()` assertion。

### shader.h 新增

`ldst_unit` 新增：`std::set<const mem_fetch*> m_pcal_bypass_mfs;`

`shader_core_ctx` 新增：
```cpp
bool pcal_is_warp_low_priority(unsigned warp_id) const { ... }
void pcal_count_bypass() { m_pcal_bypass_count++; }
```

## 验证结果

Config: `SM7_QV100_pcal_policy_on`（enable_pcal=1, enable_bypass=1, threshold=50, window=8）

| Workload | baseline | policy_on | delta | bypass_count |
|----------|----------|-----------|-------|-------------|
| vecadd | 5569 | 5548 | -0.4% | 32 |
| rodinia_hotspot | 6931 | 6919 | -0.2% | 682 |
| rodinia_srad_v2 | 8236 | 8166 | -0.85% | 3328 |
| polybench_fdtd2d | 5840 | 5798 | -0.72% | 1008 |

feature_off (noop_off) vecadd = 5569 ✓

## 近似说明

- 全部 warp miss_rate > 50%（tiny workload），bypass 触发较保守
- cycle 减少幅度小（<1%）符合预期：bypass 减少 L1D 压力但也增加 L2 traffic
- bypass 只对 global load，store 不 bypass（store 走 WRITE_ACK 路径）
