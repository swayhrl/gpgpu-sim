# PCAL-ONE-SHOT-V1：Priority-Based Cache Allocation approximate reproduction

> 用途：把本文件内容作为 Claude 的执行提示词，或让 Claude 直接读取：
>
> ```text
> 请读取并严格执行：
> tools/paper_repro/task_prompts/pcal_one_shot_v1.md
> 
> 注意：
> - 当前工作目录必须是 /workspace/repos/gpgpu-sim_distribution
> - pcal_reading 已提交，不要重复做 reading
> - 按文件中的 Phase 和 checkpoint 执行
> - 不自动 push/tag/save-session
> - 执行前先 summarize 你理解的 Phase 1-6 和 stop rules，确认没有把任务理解成自研 cache policy，然后开始 Phase 1
> ```

---

## 给 Claude 的完整提示词

```text
PCAL-ONE-SHOT-V1：Priority-Based Cache Allocation approximate reproduction

当前工作目录必须是：
/workspace/repos/gpgpu-sim_distribution

当前状态：
PCAL reading stage 已经手动完成并提交：
- docs/papers/pcal_reading_notes.md
- docs/papers/pcal_repro_plan.md
- experiments/paper-pcal/

本任务从 Phase 1 no-op config + stats 开始，不要重复生成 reading notes。

本任务目标：
对 PCAL：Priority-Based Cache Allocation in Throughput Processors（HPCA 2015）做 approximate reproduction。
目标不是 100% faithful reproduction，而是跑通机制链路：

no-op config/stats
→ telemetry
→ would-change
→ minimal cache allocation/bypass mechanism（如安全可行）
→ focused validation
→ final report

在整体规划中的位置：
- CCWS 已作为第一篇手动小 Round 复现样板完成；
- DAWS 已作为第二篇自动化流程试跑完成；
- PCAL reading 已完成；
- tools/paper_repro 已具备 L3-lite+ supervisor / queue / stop rules；
- 本轮是一次“受控大任务”：让 Claude 在明确路线图、checkpoint、commit 边界下连续推进 PCAL；
- 本轮不是自研 cache policy，不要混入 hrl/idea/cache-policy-experiments-v0；
- 本轮也不是继续改 CCWS / DAWS。

执行总规则：
- 全部用中文输出，不要使用韩文或日文；代码标识符、文件名、config knob、workload 名称保持英文。
- 不追求 100% faithful PCAL；允许 engineering approximation，但必须文档说明。
- 每个 Phase 开始前，先确认本 Phase 目标、允许改动、禁止事项。
- 每个 Phase 完成后必须输出 checkpoint summary。
- 每个 Phase 完成且验证通过后，可以本地 git commit。
- 不自动 push。
- 不自动 tag。
- 不运行 /save-session。
- 不运行 full standard / extended。
- 不做大参数 sweep。
- 不改与 PCAL 无关的源码。
- 不删除、不重写 CCWS / DAWS 相关文件。
- 如果任一 Phase 超过 10 分钟，暂停当前扩展，输出 checkpoint summary，再决定是否继续。
- 如果出现 hang/deadlock/extreme slowdown，停止扩大实验，记录并收尾。
- 如果需要大范围重构 cache hierarchy / memory system，停止并文档化，不要硬做。
- 如果 Phase 4 minimal mechanism 需要大范围改 cache internals，宁可停在 would-change + final report，也不要硬做。

开始前检查：
1. 执行：
   pwd
   git branch --show-current
   git status --short
   git diff --stat
   git log --oneline -5

2. pwd 必须是：
   /workspace/repos/gpgpu-sim_distribution

3. 当前分支必须是：
   hrl/paper/pcal-repro-v0

4. git status --short 必须干净。

5. 确认 reading 产物存在：
   docs/papers/pcal_reading_notes.md
   docs/papers/pcal_repro_plan.md
   experiments/paper-pcal/

6. 如果不满足，停止并报告原因，不要继续。

版本管理规则：
- 每个 Phase 通过后允许本地 commit。
- commit 前必须执行：
  git status --short
  git diff --stat
- 每个 commit message 按本提示词给定建议。
- 不自动 push。
- 不自动 tag。
- 如果某个 Phase 没通过，不要 commit，写清楚原因并停止。
- 禁止删除 CCWS / DAWS 相关文件。
- 禁止修改其他 paper branch 的语义文档，除非只是 CLAUDE.md 状态记录。
- 每个 Phase 的 commit 只包含本 Phase 相关文件。
- 如果某 Phase 改动失败且要回退，只回退本 Phase 改动。

PCAL approximate reproduction 原则：
1. 优先复现机制链路，不追求论文 exact numbers。
2. 允许将 PCAL 的 right-to-cache / priority allocation 近似为：
   - per-warp/per-CTA cache pressure telemetry；
   - high-priority vs low-priority allocation decision；
   - would-bypass / would-allocate telemetry；
   - minimal L1D admission/bypass 或 cache allocation proxy。
3. 如果 GPGPU-Sim 中没有安全、局部、可解释的 cache insertion/bypass hook，不要硬改大范围 cache path；
   可以停在 would-change telemetry + final report，并说明 limitation。
4. focused validation 优先，不跑 standard。
5. workload 使用 WORKLOAD-AUDIT 结论：
   focused candidates：
   - rodinia_hotspot
   - rodinia_srad_v2
   - polybench_fdtd2d
   - mutual_tiled
   - polybench_2dconv
   - strided_access
   - parboil_histo

   controls：
   - vecadd
   - polybench_gemm
   - mutual_naive
   - rodinia_backprop
   - atomic_contention

============================================================
Phase 0：preflight only
============================================================

目标：
只确认 PCAL reading 已完成、分支干净、实验目录存在。

允许：
- 只读检查；
- 如 experiments/paper-pcal/round_state.yaml 缺失，可补一个 reading 状态文件；
- 如 CLAUDE.md 需要记录“PCAL reading 已完成”，可最小更新。

禁止：
- 重写 reading notes；
- 改 src/；
- 跑 workload；
- 进入机制实现。

需要检查：
1. docs/papers/pcal_reading_notes.md
2. docs/papers/pcal_repro_plan.md
3. experiments/paper-pcal/
4. tools/paper_repro/papers/pcal.yaml
5. tools/paper_repro/preplans/pcal_preplan.yaml

round_state.yaml 如需补充，至少包含：
- paper: pcal
- round: Phase 0
- stage: preflight
- status
- source_changed: false
- experiments_run: false
- reading_stage_already_committed: true
- recommend_next: Phase 1

成功标准：
- docs/papers/pcal_reading_notes.md 存在；
- docs/papers/pcal_repro_plan.md 存在；
- experiments/paper-pcal/ 存在；
- git status 干净或只有 Phase 0 最小文档补充；
- 可以进入 Phase 1。

如果 Phase 0 只是检查，不需要 commit。

如果补了 round_state.yaml / CLAUDE.md，可以 commit：
git commit -m "pcal: refresh reading stage state"

============================================================
Phase 1：no-op config + stats
============================================================

目标：
添加 PCAL 默认关闭的 config knobs 和 paper_pcal_* stats，占位但不改变行为。

允许修改：
- src/gpgpu-sim/shader.h
- src/gpgpu-sim/shader.cc
- src/gpgpu-sim/gpu-sim.cc
- 如必须，可少量修改与 stats printing 直接相关文件。

禁止：
- 不实现 telemetry；
- 不实现 would-change；
- 不实现真实 bypass / allocation；
- 不改变 scheduler/cache/timing 行为；
- 不跑 standard / extended。

建议 knobs，默认全部关闭：
- gpgpu_enable_pcal = 0
- gpgpu_pcal_enable_telemetry = 0
- gpgpu_pcal_enable_would_change = 0
- gpgpu_pcal_enable_policy = 0
- gpgpu_pcal_debug = 0
- gpgpu_pcal_priority_threshold = 0 或安全默认值
- gpgpu_pcal_bypass_threshold = 0 或安全默认值

建议 stats：
- paper_pcal_enabled
- paper_pcal_cache_pressure_sample
- paper_pcal_priority_update
- paper_pcal_would_allocate
- paper_pcal_would_bypass
- paper_pcal_policy_bypass
- paper_pcal_policy_allocate

要求：
- feature_off 行为必须不变；
- feature_on_noop 行为也必须不变；
- 行为计数保持 0，除了 enabled 类状态。

配置：
创建：
- configs/hrl-repro/SM7_QV100_pcal_noop_off/
- configs/hrl-repro/SM7_QV100_pcal_noop_on/

验证：
只跑 quick subset，不跑 standard：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2

如时间紧，至少：
- vecadd
- rodinia_hotspot

需要产出：
1. docs/papers/pcal_round_01_noop_config.md
2. experiments/paper-pcal/noop_behavior_check.csv
3. 更新 experiments/paper-pcal/config_matrix.csv
4. 更新 docs/papers/pcal_repro_plan.md
5. 更新 tools/paper_repro/papers/pcal.yaml
6. 更新 experiments/paper-pcal/round_state.yaml
7. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 1
- stage: noop_config
- status
- branch
- source_changed: true
- experiments_run
- workloads_run
- feature_off_unchanged
- feature_on_noop_unchanged
- paper_pcal_stats_zero
- recommend_next
- commit_recommended
- suggested_commit

成功标准：
- 编译通过；
- feature_off cycle 与 baseline 一致；
- feature_on_noop cycle 与 feature_off 一致；
- 除 enabled 状态外，paper_pcal_* 行为计数为 0；
- 没有真实 PCAL 行为改动。

Phase 1 通过后 commit：
git commit -m "pcal: add no-op config and stats"

============================================================
Phase 2：cache pressure / priority telemetry instrumentation-only
============================================================

目标：
实现 passive telemetry，用于观测 PCAL 所需的 cache pressure / priority signal。
不改变 cache 行为，不改变 scheduler 行为。

允许：
- 添加 per-warp / per-core / per-SM telemetry counters；
- 统计 L1D miss/hit、load activity、cache pressure proxy；
- 统计 per-warp 或 per-CTA priority proxy；
- 使用近似但可解释的 cache pressure signal。

禁止：
- 真实 bypass；
- 真实 allocation decision；
- 改 cache replacement；
- 改 scheduler；
- 改 memory timing；
- 跑 standard / extended。

建议 telemetry signal：
- per-warp load count
- per-warp L1D miss count 或 miss-side proxy
- per-warp cache pressure score
- high_priority_warp_count
- low_priority_warp_count
- paper_pcal_cache_pressure_sample
- paper_pcal_priority_update

如果 exact PCAL priority/right-to-cache 定义难以直接获得：
- 使用可解释的 approximation；
- 在文档中说明 approximation；
- 不要深挖到大范围 simulator 重构。

配置：
创建：
- configs/hrl-repro/SM7_QV100_pcal_telemetry_on/

验证 workloads：
focused 小集合：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2
- polybench_fdtd2d

如某个 workload 不可用，记录原因，不扩大。

要求：
- telemetry_on sim_cycle 不变；
- policy_bypass / policy_allocate 仍为 0；
- cache pressure / priority telemetry 有信号或可解释为无信号；
- control workload 应该低信号或可解释。

需要产出：
1. docs/papers/pcal_round_02_cache_pressure_telemetry.md
2. experiments/paper-pcal/cache_pressure_telemetry_check.csv
3. 更新 experiments/paper-pcal/config_matrix.csv
4. 更新 docs/papers/pcal_repro_plan.md
5. 更新 tools/paper_repro/papers/pcal.yaml
6. 更新 experiments/paper-pcal/round_state.yaml
7. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 2
- stage: cache_pressure_telemetry
- status
- branch
- source_changed: true
- experiments_run
- workloads_run
- feature_off_unchanged
- telemetry_on_unchanged
- cache_pressure_signal_summary
- priority_signal_summary
- recommend_next
- commit_recommended
- suggested_commit

成功标准：
- 编译通过；
- feature_off 不变；
- telemetry_on 不变；
- telemetry signal 合理；
- 无真实 policy 行为。

Phase 2 通过后 commit：
git commit -m "pcal: add cache pressure telemetry"

============================================================
Phase 3：would-change right-to-cache / bypass telemetry
============================================================

目标：
基于 Phase 2 signal，计算 PCAL approximate right-to-cache / would-bypass / would-allocate decision。
仍然不真实改变 cache 行为。

允许：
- 添加 would_allocate / would_bypass 逻辑；
- 计算 high-priority / low-priority allocation decision；
- 记录 would-change counters；
- 如需新增安全阈值 knob，可最小新增。

禁止：
- 真实 bypass；
- 真实 cache insertion/admission 改动；
- 改 replacement；
- 改 scheduler；
- 跑 standard / extended；
- 为了制造信号做大 sweep。

approximation 建议：
- 高 priority warp：would_allocate；
- 低 priority warp 且 cache pressure 超阈值：would_bypass；
- controls 应低信号；
- cache-sensitive workloads 应出现 would_bypass / would_allocate 差异。

配置：
创建：
- configs/hrl-repro/SM7_QV100_pcal_would_change_on/

验证：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2
- polybench_fdtd2d
- mutual_tiled

要求：
- sim_cycle 不变；
- paper_pcal_would_bypass / would_allocate 有可解释信号；
- paper_pcal_policy_bypass / policy_allocate 仍为 0。

需要产出：
1. docs/papers/pcal_round_03_would_change.md
2. experiments/paper-pcal/would_change_check.csv
3. 更新 experiments/paper-pcal/config_matrix.csv
4. 更新 docs/papers/pcal_repro_plan.md
5. 更新 tools/paper_repro/papers/pcal.yaml
6. 更新 experiments/paper-pcal/round_state.yaml
7. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 3
- stage: would_change
- status
- branch
- source_changed: true
- experiments_run
- workloads_run
- behavior_unchanged
- would_allocate_signal_summary
- would_bypass_signal_summary
- recommend_next
- commit_recommended
- suggested_commit

成功标准：
- would-change 有合理信号；
- 行为不变；
- 无真实 cache policy 改动；
- 可判断是否进入 minimal mechanism。

Phase 3 通过后 commit：
git commit -m "pcal: add would-change cache allocation telemetry"

============================================================
Phase 4：minimal PCAL mechanism，谨慎执行
============================================================

目标：
如果 Phase 3 would-change signal 合理，尝试实现最小真实 PCAL approximation。
只在 gpgpu_pcal_enable_policy=1 时改变行为。

重要：
这是高风险阶段。进入前必须先输出 checkpoint：
- Phase 2/3 的信号是否足够；
- 计划改哪些文件；
- 找到的具体 hook 点是什么；
- 是否存在局部 hook；
- 是否可能需要大范围 cache 重构。

强制停机规则：
如果你判断 Phase 4 minimal mechanism 需要改动 cache replacement / fill path / tag array / mshr / miss queue / memory return path 超过 2 个核心文件，或者需要重构 cache hierarchy，请立即停止 Phase 4：

1. 不要继续实现；
2. 不要硬做；
3. 不要为了出信号改大范围路径；
4. 文档化 limitation；
5. 保留 Phase 3 would-change 作为 PCAL approximate reproduction 的主要结果；
6. 直接进入 Phase 6 final report。

允许的最小实现方向：
1. 优先选择局部、可回退的 L1D admission / bypass proxy；
2. 低 priority 或 high pressure 下，对部分 cache allocation 做 bypass / no-allocate proxy；
3. 高 priority 继续 allocate；
4. 默认关闭；
5. feature_off 必须不变。

禁止：
- 大范围重构 cache hierarchy；
- 大范围改 replacement policy；
- 改 DRAM / interconnect；
- 改 benchmark；
- 跑 standard；
- 为了出信号硬调过多参数；
- 在不理解 hook 语义时继续修改；
- 修改超过预期模块后继续推进。

验证：
quick/focused 小集合：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2
- polybench_fdtd2d

要求：
- feature_off 不变；
- policy_on 有真实 policy_bypass 或 policy_allocate signal；
- 无 hang/deadlock；
- 如果出现 extreme slowdown，停止扩大实验；
- 如果没有安全实现点，文档化 limitation，跳到 Phase 6 final report。

需要产出：
1. configs/hrl-repro/SM7_QV100_pcal_policy_on/
2. docs/papers/pcal_round_04_minimal_policy.md
3. experiments/paper-pcal/minimal_policy_check.csv
4. 更新 experiments/paper-pcal/config_matrix.csv
5. 更新 docs/papers/pcal_repro_plan.md
6. 更新 tools/paper_repro/papers/pcal.yaml
7. 更新 experiments/paper-pcal/round_state.yaml
8. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 4
- stage: minimal_policy
- status
- branch
- source_changed: true
- experiments_run
- workloads_run
- hook_point
- changed_files
- feature_off_unchanged
- policy_signal_summary
- hang_or_deadlock
- slowdown_summary
- limitation_if_stopped
- recommend_next
- commit_recommended
- suggested_commit

成功标准：
- 编译通过；
- feature_off 不变；
- policy_on 有真实 signal；
- 无 hang/deadlock；
- 改动范围可解释。

Phase 4 通过后 commit：
git commit -m "pcal: add minimal cache allocation policy"

如果 Phase 4 因高风险停止：
- 不 commit 失败源码；
- 回退源码或保留文档化状态；
- 输出明确 limitation；
- 进入 Phase 6 final report。

============================================================
Phase 5：focused validation
============================================================

目标：
不改机制，只做 focused validation。
不跑 standard。

前置：
只有在 Phase 4 成功实现 minimal policy 时，才运行 policy_on focused validation。
如果 Phase 4 被安全停机，则可对 telemetry / would_change 结果做 focused summary，但不要伪装成 policy validation。

配置比较：
- feature_off / baseline
- telemetry_on 或 would_change_on
- policy_on，如果 Phase 4 成功

workloads：
focused：
- rodinia_hotspot
- rodinia_srad_v2
- polybench_fdtd2d
- mutual_tiled
- polybench_2dconv
- strided_access
- parboil_histo

controls：
- vecadd
- polybench_gemm
- mutual_naive

如时间不够，先 focused 5 个 + controls 2 个。

重点字段：
- workload
- config
- pass
- sim_cycle
- baseline_cycle
- cycle_delta
- cache_pressure_score
- would_allocate
- would_bypass
- policy_allocate
- policy_bypass
- notes

判断：
1. cache-sensitive workload 是否有 signal；
2. control workload 是否少触发；
3. policy_on 是否有可解释 cycle_delta；
4. 是否出现 over-bypass / extreme slowdown；
5. 是否足够进入 final report。

需要产出：
1. docs/papers/pcal_round_05_focused_validation.md
2. experiments/paper-pcal/focused_validation.csv
3. 更新 experiments/paper-pcal/config_matrix.csv
4. 更新 docs/papers/pcal_repro_plan.md
5. 更新 experiments/paper-pcal/round_state.yaml
6. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 5
- stage: focused_validation
- status
- branch
- source_changed: false
- experiments_run
- workloads_run
- configs_run
- feature_off_unchanged
- signal_summary
- policy_validation_available
- slowdown_summary
- recommend_next
- commit_recommended
- suggested_commit

成功标准：
- 不改 src/；
- focused validation 完成；
- 结果可解释；
- 不跑 standard；
- 能判断 final report。

Phase 5 通过后 commit：
git commit -m "pcal: validate approximate policy on focused workloads"

============================================================
Phase 6：final report
============================================================

目标：
总结 PCAL approximate reproduction。
无论 Phase 4 是否成功，都要清楚写出当前复现状态。

不改 src。
不跑实验。

final report 结构：
1. Scope and Goal
2. PCAL Mechanism Recap
3. GPGPU-Sim Mapping
4. Implemented Stages
5. What Was Successfully Reproduced
6. Approximation and Deviations
7. Key Experimental Findings
8. Interpretation
9. Reproduction Status
10. Recommended Next Steps

必须明确：
- 是否完成 mechanism chain；
- 是否 faithful reproduction；
- telemetry / would-change / minimal policy 哪些完成；
- 为什么结果可能不等同原论文；
- 是否建议停止 PCAL；
- 是否建议进入 cache policy 自研；
- 如果 Phase 4 停在 limitation，要清楚说明原因；
- 如果 Phase 4 成功，要说明 focused validation 是否支持大致预期。

需要产出：
1. docs/papers/pcal_final_reproduction_report.md
2. experiments/paper-pcal/final_summary.csv
3. 更新 docs/papers/pcal_repro_plan.md
4. 更新 tools/paper_repro/papers/pcal.yaml
5. 更新 experiments/paper-pcal/round_state.yaml
6. 更新 CLAUDE.md

round_state.yaml 至少包含：
- paper: pcal
- round: Phase 6
- stage: final_report
- status
- branch
- source_changed: false
- experiments_run: false
- final_report_created: true
- mechanism_chain_reproduced
- faithful_reproduction_status
- telemetry_status
- would_change_status
- minimal_policy_status
- focused_validation_status
- recommend_next
- commit_recommended
- suggested_commit

Phase 6 通过后 commit：
git commit -m "pcal: add final reproduction report"

============================================================
每 10 分钟 checkpoint 格式
============================================================

如果任一阶段超过 10 分钟，请暂停扩展并输出：

---
Checkpoint Summary
- 当前 Phase：
- 已完成：
- 正在做：
- 已修改文件：
- 已运行命令：
- 已跑 workload/config：
- 当前 git status --short：
- 是否有异常：
- 是否建议继续：
- 是否建议停止 review：
---

不要在 checkpoint 后继续扩大任务，除非当前阶段已经接近完成且没有风险。

============================================================
每个 Phase 结束 checkpoint 格式
============================================================

每个 Phase 结束时输出：

---
Phase Summary
- Phase：
- 是否完成：
- 修改文件：
- 运行命令：
- workload/config：
- 核心结果：
- 是否满足成功标准：
- 是否已 commit：
- commit message：
- 是否建议进入下一 Phase：
- 是否需要 GPT/Codex review：
---

============================================================
最终输出要求
============================================================

任务结束时输出：

1. PCAL 复现进行到哪个 Phase。
2. 每个 Phase 是否完成。
3. 生成了哪些 commits。
4. 是否有未提交改动。
5. 是否存在 src 改动。
6. 是否出现 hang/deadlock/extreme slowdown。
7. 是否建议进入 GPT/Codex review。
8. 是否建议后续继续 PCAL，还是收束。
9. 不要 push，不要 tag，不要 /save-session。
```