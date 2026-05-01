# Result CSV Fields

统一 CSV 字段建议，适用于所有论文复现实验结果。

## 核心字段（所有论文必须包含）

| 字段 | 类型 | 说明 |
|------|------|------|
| `paper` | string | 论文 key，如 `ccws` |
| `round` | string | 轮次，如 `AI`、`Y` |
| `stage` | string | stage 名称，如 `focused_validation` |
| `workload` | string | workload 名称 |
| `set` | string | workload set：`quick` / `focused` / `standard` |
| `config` | string | config 目录名 |
| `pass` | int | 1 = 通过，0 = 失败 |
| `sim_cycle` | int | 本次运行 sim_cycle |
| `baseline_cycle` | int | feature_off baseline sim_cycle |
| `cycle_delta` | string | 如 `+412` 或 `0` |
| `cycle_delta_pct` | string | 如 `+6%` 或 `0%` |
| `main_signal` | int | 论文主要 signal 计数（如 `load_gate_block`） |
| `notes` | string | 简短说明 |

## 扩展字段（按论文需要添加）

不同论文可在核心字段后追加论文特定字段，例如：

**CCWS 扩展字段**：
- `vta_hit`：VTA probe hit 次数
- `lls_update`：LLS score update 次数
- `would_gate_block`：would-gate telemetry block 次数
- `load_gate_block`：真实 load gate block 次数
- `lls_hit_increment`：本次运行的 lls_hit_increment 参数
- `lg_score_threshold`：本次运行的 lg_score_threshold 参数

## 示例行

```csv
paper,round,stage,workload,set,config,pass,sim_cycle,baseline_cycle,cycle_delta,cycle_delta_pct,main_signal,notes
ccws,AI,focused_validation,rodinia_hotspot,focused,SM7_QV100_ccws_load_gate_th100,1,7343,6931,+412,+6%,4595,conservative; gating active
ccws,AI,focused_validation,strided_access,focused,SM7_QV100_ccws_load_gate_th100,1,5825,5825,0,0%,0,no gating (low vta_hit)
```

## 注意事项

- `pass=1` 表示 gpgpusim_exit=1（模拟完整跑完），不代表结果符合预期
- `main_signal` 字段含义因论文而异，在 paper.yaml 的 `mechanism_chain` 中说明
- `baseline_cycle` 应来自 feature_off 运行，不是原论文数字
