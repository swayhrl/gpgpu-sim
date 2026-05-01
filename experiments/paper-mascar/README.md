# Mascar Reproduction Experiments

**论文**：Mascar: Speeding up GPU Warps by Reducing Memory Pitstops (HPCA 2015)
**分支**：hrl/paper/mascar-repro-v0
**实验目录**：experiments/paper-mascar/

---

## 目录结构

```
experiments/paper-mascar/
├── README.md                          # 本文件
├── round_state.yaml                   # 当前复现状态
├── config_matrix.csv                  # 各阶段 config 矩阵
├── mascar_one_shot_checkpoint_log.md  # 轻量 checkpoint log
├── noop_behavior_check.csv            # Phase 1 验证结果
├── memory_pressure_telemetry_check.csv # Phase 2 验证结果
├── would_change_check.csv             # Phase 3 验证结果
├── minimal_policy_check.csv           # Phase 4 验证结果
├── focused_validation.csv             # Phase 5 验证结果
└── final_summary.csv                  # Phase 6 总结
```

---

## 验证说明

- 所有实验只跑 focused / quick workload 集合，不跑 standard；
- baseline 参考：vecadd sim_cycle = 5569；
- feature_off 必须与 baseline cycle 一致；
- 如出现 hang/deadlock 立即停止。
