# MASCAR-ONE-SHOT-V1：Mascar approximate reproduction

建议保存到仓库内：

```text
tools/paper_repro/task_prompts/mascar_one_shot_v1.md
```

给 Claude 的短提示词：

```text
请读取并严格执行：
tools/paper_repro/task_prompts/mascar_one_shot_v1.md

执行前请先 summarize 你理解的 Phase 0-6、stop rules、版本管理规则和 Phase 4 风险边界，确认：
1. 当前工作目录是 /workspace/repos/gpgpu-sim_distribution；
2. 当前分支是 hrl/paper/mascar-repro-v0；
3. 本任务是 Mascar approximate reproduction，不是 PCAL/CCWS/DAWS，也不是自研 cache policy；
4. 本任务允许推进到完整 minimal mechanism，但如果需要真实 replay/re-execution 或大范围 memory pipeline 重构，必须停下并文档化 limitation；
5. 不自动 push/tag/save-session；
6. 每个 Phase 完成后可以本地 commit；
7. 不跑 standard/extended。

确认理解后，从 Phase 0 preflight/reading 开始执行。
全程用中文输出，不要使用韩文或日文；代码标识符、文件名、config knob、workload 名称保持英文。
```

---

## 给 Claude 的完整提示词

```text
MASCAR-ONE-SHOT-V1：Mascar approximate reproduction

当前工作目录必须是：
/workspace/repos/gpgpu-sim_distribution

当前分支必须是：
hrl/paper/mascar-repro-v0

本任务目标：
对 Mascar: Speeding up GPU Warps by Reducing Memory Pitstops（HPCA 2015）做 approximate reproduction。
目标不是 100% faithful reproduction，而是跑通机制链路：

reading/plan
→ no-op config/stats
→ memory pressure / pitstop telemetry
→ would-change scheduling telemetry
→ minimal real scheduling/pitstop-reduction mechanism（如安全可行）
→ focused validation
→ final report

本任务希望像 PCAL one-shot 一样，在明确 stop rules 和 checkpoint 约束下，尽量推进到完整 minimal mechanism。
但必须注意：Mascar 的原始机制可能涉及 memory saturation、warp scheduling、memory request prioritization、甚至 re-execution/replay 语义。GPGPU-Sim 中如果没有安全、局部、可解释的 hook，就不要硬做真实 replay/re-execution。

在整体规划中的位置：
- CCWS 已作为第一篇手动小 Round 复现样板完成；
- DAWS 已作为第二篇自动化流程试跑完成；
- PCAL 已完成 one-shot approximate reproduction 并收束；
- tools/paper_repro 已具备 L3-lite+ supervisor / queue / stop rules；
- 本轮是 Mascar 的“受控大任务”：让 Claude 在明确路线图、checkpoint、commit 边界下连续推进；
- 本轮不是自研 cache policy，不要混入 hrl/idea/cache-policy-experiments-v0；
- 本轮不是继续改 PCAL/CCWS/DAWS。

执行总规则：
- 全部用中文输出，不要使用韩文或日文；代码标识符、文件名、config knob、workload 名称保持英文。
- 不追求 100% faithful Mascar；允许 engineering approximation，但必须文档说明。
- 每个 Phase 开始前，先确认本 Phase 目标、允许改动、禁止事项。
- 每个 Phase 完成后必须输出 checkpoint summary。
- 每个 Phase 完成且验证通过后，可以本地 git commit。
- 不自动 push。
- 不自动 tag。
- 不运行 /save-session。
- 不运行 full standard / extended。
- 不做大参数 sweep。
- 不改与 Mascar 无关的源码。
- 不删除、不重写 PCAL / CCWS / DAWS 相关文件。
- 如果任一 Phase 超过 10 分钟，暂停当前扩展，输出 checkpoint summary，再决定是否继续。
- 如果出现 hang/deadlock/extreme slowdown，停止扩大实验，记录并收尾。
- 如果需要大范围重构 scheduler / ldst_unit / scoreboard / memory queue / cache miss return path / interconnect / DRAM，停止并文档化，不要硬做。
- 如果 Phase 4 minimal mechanism 需要真实 replay/re-execution 或 memory pipeline 大范围重构，宁可停在 would-change + final report，也不要硬做。

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
   hrl/paper/mascar-repro-v0

4. git status --short 必须干净。

5. 确认基础文件存在：
   tools/paper_repro/papers/mascar.yaml
   tools/paper_repro/preplans/mascar_preplan.yaml
   docs/papers/mascar_auto_reading_prompt_preview.md

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
- 禁止删除 PCAL / CCWS / DAWS 相关文件。
- 禁止修改其他 paper branch 的语义文档，除非只是 CLAUDE.md 状态记录。
- 每个 Phase 的 commit 只包含本 Phase 相关文件。
- 如果某 Phase 改动失败且要回退，只回退本 Phase 改动。

Mascar approximate reproduction 原则：
1. 优先复现机制链路，不追求论文 exact numbers。
2. 允许将 Mascar 的 memory pitstop / saturation / warp scheduling 近似为：
   - per-warp memory pressure telemetry；
   - per-warp memory stall / long-latency activity；
   - per-core / per-SM memory saturation proxy；
   - would-prioritize / would-delay telemetry；
   - minimal scheduler priority/delay 或 issue-order proxy。
3. 如果无法安全实现真实 re-execution/replay，不要硬做；可以用 scheduler prioritization / delay proxy 作为 minimal mechanism。
4. focused validation 优先，不跑 standard。
5. workload 使用 WORKLOAD-AUDIT 和前面论文经验：
   focused candidates：
   - rodinia_hotspot
   - rodinia_srad_v2
   - rodinia_bfs
   - strided_access
   - polybench_fdtd2d
   - polybench_2dconv
   - mutual_tiled
   - parboil_histo

   controls：
   - vecadd
   - polybench_gemm
   - mutual_naive
   - rodinia_backprop

============================================================
Phase 0：reading / plan / preflight
============================================================

目标：
完成 Mascar reading notes 和 reproduction plan，并确认当前分支干净。

允许：
- 读取已有 mascar.yaml / mascar_preplan.yaml / reading prompt preview；
- 总结论文机制；
- 创建 experiments/paper-mascar/；
- 创建 config_matrix.csv、round_state.yaml；
- 更新 CLAUDE.md。

禁止：
- 改 src/；
- 跑 workload；
- 进入机制实现；
- 编造论文细节。若论文信息不足，明确标注 unknown / approximation，不要假装 faithful。

需要产出：
1. docs/papers/mascar_reading_notes.md
2. docs/papers/mascar_repro_plan.md
3. experiments/paper-mascar/README.md
4. experiments/paper-mascar/config_matrix.csv
5. experiments/paper-mascar/round_state.yaml
6. 更新 tools/paper_repro/papers/mascar.yaml
7. 更新 CLAUDE.md
8. 创建轻量 checkpoint log：
   experiments/paper-mascar/mascar_one_shot_checkpoint_log.md

reading notes 至少包含：
- 研究对象；
- 动机；
- 目前问题；
- Mascar 的核心方法；
- 与 DAWS / CCWS / PCAL 的关系；
- 可能映射到 GPGPU-Sim 的模块；
- 可接受的 approximation；
- 高风险点。

round_state.yaml 至少包含：
- paper: mascar
- round: Phase 0
- stage: reading
- status
- source_changed: false
- experiments_run: false
- recommend_next: Phase 1

成功标准：
- reading notes 和 repro plan 存在；
- 实验目录存在；
- 没有 src 改动；
- 可以进入 Phase 1。

Phase 0 通过后 commit：
git commit -m "mascar: add reading notes and reproduction plan"

============================================================
Phase 1：no-op config + stats
============================================================

目标：
添加 Mascar 默认关闭的 config knobs 和 paper_mascar_* stats，占位但不改变行为。

允许修改：
- src/gpgpu-sim/shader.h
- src/gpgpu-sim/shader.cc
- src/gpgpu-sim/gpu-sim.cc
- 如必须，可少量修改与 stats printing 直接相关文件。

禁止：
- 不实现 telemetry；
- 不实现 would-change；
- 不实现真实 scheduling policy；
- 不实现 replay/re-execution；
- 不改变 scheduler/cache/timing 行为；
- 不跑 standard / extended。

建议 knobs，默认全部关闭：
- gpgpu_enable_mascar = 0
- gpgpu_mascar_enable_telemetry = 0
- gpgpu_mascar_enable_would_change = 0
- gpgpu_mascar_enable_policy = 0
- gpgpu_mascar_debug = 0
- gpgpu_mascar_mem_pressure_threshold = 安全默认值
- gpgpu_mascar_delay_threshold = 安全默认值
- gpgpu_mascar_priority_boost = 安全默认值

建议 stats：
- paper_mascar_enabled
- paper_mascar_mem_pressure_sample
- paper_mascar_saturation_event
- paper_mascar_pitstop_event
- paper_mascar_would_prioritize
- paper_mascar_would_delay
- paper_mascar_policy_prioritize
- paper_mascar_policy_delay

要求：
- feature_off 行为必须不变；
- feature_on_noop 行为也必须不变；
- 行为计数保持 0，除了 enabled 类状态。

配置：
创建：
- configs/hrl-repro/SM7_QV100_mascar_noop_off/
- configs/hrl-repro/SM7_QV100_mascar_noop_on/

验证：
只跑 quick subset，不跑 standard：
- vecadd
- rodinia_hotspot
- rodinia_bfs

如时间紧，至少：
- vecadd
- rodinia_hotspot

需要产出：
1. docs/papers/mascar_round_01_noop_config.md
2. experiments/paper-mascar/noop_behavior_check.csv
3. 更新 experiments/paper-mascar/config_matrix.csv
4. 更新 docs/papers/mascar_repro_plan.md
5. 更新 tools/paper_repro/papers/mascar.yaml
6. 更新 experiments/paper-mascar/round_state.yaml
7. 更新 CLAUDE.md
8. 追加 checkpoint log

成功标准：
- 编译通过；
- feature_off cycle 与 baseline 一致；
- feature_on_noop cycle 与 feature_off 一致；
- 除 enabled 状态外，paper_mascar_* 行为计数为 0；
- 没有真实 Mascar 行为改动。

Phase 1 通过后 commit：
git commit -m "mascar: add no-op config and stats"

============================================================
Phase 2：memory pressure / pitstop telemetry instrumentation-only
============================================================

目标：
实现 passive telemetry，用于观测 Mascar 所需的 memory pressure / memory pitstop / saturation signal。
不改变 scheduler 行为，不改变 memory/cache timing 行为。

允许：
- 添加 per-warp / per-core / per-SM telemetry counters；
- 统计 memory instruction issue activity；
- 统计 memory stall / long-latency / blocked issue proxy；
- 统计 memory queue pressure proxy；
- 统计 saturation event proxy；
- 使用近似但可解释的 pitstop signal。

禁止：
- 真实 scheduling priority；
- 真实 delay；
- 真实 replay/re-execution；
- 改 cache replacement；
- 改 memory queue 行为；
- 改 scoreboard 语义；
- 跑 standard / extended。

建议 telemetry signal：
- per-warp memory instruction count
- per-warp memory stall / blocked count proxy
- memory pressure score
- saturation event count
- pitstop event count
- paper_mascar_mem_pressure_sample
- paper_mascar_saturation_event
- paper_mascar_pitstop_event

配置：
创建：
- configs/hrl-repro/SM7_QV100_mascar_telemetry_on/

验证 workloads：
focused 小集合：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2
- rodinia_bfs
- strided_access

要求：
- telemetry_on sim_cycle 不变；
- policy_prioritize / policy_delay 仍为 0；
- memory pressure / saturation / pitstop telemetry 有信号或可解释为无信号。

需要产出：
1. docs/papers/mascar_round_02_memory_pressure_telemetry.md
2. experiments/paper-mascar/memory_pressure_telemetry_check.csv
3. 更新 experiments/paper-mascar/config_matrix.csv
4. 更新 docs/papers/mascar_repro_plan.md
5. 更新 tools/paper_repro/papers/mascar.yaml
6. 更新 experiments/paper-mascar/round_state.yaml
7. 更新 CLAUDE.md
8. 追加 checkpoint log

成功标准：
- 编译通过；
- feature_off 不变；
- telemetry_on 不变；
- telemetry signal 合理；
- 无真实 policy 行为。

Phase 2 通过后 commit：
git commit -m "mascar: add memory pressure telemetry"

============================================================
Phase 3：would-change prioritization / delay telemetry
============================================================

目标：
基于 Phase 2 signal，计算 Mascar approximate would-prioritize / would-delay / would-reduce-pitstop decision。
仍然不真实改变 scheduler 或 memory 行为。

允许：
- 添加 would_prioritize / would_delay 逻辑；
- 基于 memory pressure / pitstop score 计算 candidate warp；
- 记录 would-change counters；
- 如需新增安全阈值 knob，可最小新增。

禁止：
- 真实 scheduler priority；
- 真实 delay；
- 真实 replay/re-execution；
- 改 memory queue；
- 改 scoreboard；
- 跑 standard / extended；
- 为了制造信号做大 sweep。

配置：
创建：
- configs/hrl-repro/SM7_QV100_mascar_would_change_on/

验证：
- vecadd
- rodinia_hotspot
- rodinia_srad_v2
- rodinia_bfs
- strided_access
- polybench_fdtd2d

要求：
- sim_cycle 不变；
- paper_mascar_would_prioritize / would_delay 有可解释信号；
- paper_mascar_policy_prioritize / policy_delay 仍为 0。

需要产出：
1. docs/papers/mascar_round_03_would_change.md
2. experiments/paper-mascar/would_change_check.csv
3. 更新 experiments/paper-mascar/config_matrix.csv
4. 更新 docs/papers/mascar_repro_plan.md
5. 更新 tools/paper_repro/papers/mascar.yaml
6. 更新 experiments/paper-mascar/round_state.yaml
7. 更新 CLAUDE.md
8. 追加 checkpoint log

成功标准：
- would-change 有合理信号；
- 行为不变；
- 无真实 scheduling/memory policy 改动；
- 可判断是否进入 minimal mechanism。

Phase 3 通过后 commit：
git commit -m "mascar: add would-change scheduling telemetry"

============================================================
Phase 4：minimal Mascar mechanism，允许推进但必须谨慎
============================================================

目标：
如果 Phase 3 would-change signal 合理，尝试实现最小真实 Mascar approximation。
只在 gpgpu_mascar_enable_policy=1 时改变行为。

本任务希望像 PCAL 一样放开完整机制，因此 Phase 4 可以尝试真实 minimal mechanism。
但必须限定为局部、可回退、可解释的 scheduler/pitstop-reduction proxy。

重要：
这是最高风险阶段。进入前必须先输出 risk checkpoint，并 append 到 checkpoint log：
- Phase 2/3 的信号是否足够；
- 计划改哪些文件；
- 找到的具体 hook 点是什么；
- 是否只是 scheduler issue-order / priority / delay proxy；
- 是否涉及 replay/re-execution；
- 是否可能需要大范围 memory pipeline 重构。

强制停机规则：
如果你判断 Phase 4 minimal mechanism 需要实现真实 replay / re-execution，或者需要大范围修改 ldst_unit、memory queue、scoreboard、cache miss return path、MSHR、interconnect、DRAM 超过 2 个核心文件，请立即停止 Phase 4：

1. 不要继续实现；
2. 不要硬做；
3. 不要为了出信号改大范围路径；
4. 文档化 limitation；
5. 保留 Phase 3 would-change 作为 Mascar approximate reproduction 的主要结果；
6. 直接进入 Phase 6 final report。

允许的最小实现方向：
1. 优先选择局部、可回退的 scheduler priority / delay proxy；
2. 对 high-pitstop / high-memory-pressure warp 做临时 delay 或 deprioritize；
3. 对 lower-pressure / likely-progress warp 做 prioritize；
4. 不做真实 replay/re-execution；
5. 不改 memory request 内容；
6. 不改 cache replacement；
7. 默认关闭；
8. feature_off 必须不变。

可考虑 hook：
- scheduler_unit::cycle() 中候选 warp 选择 / issue-order 相关位置；
- 仅在 issue 前读取 per-warp Mascar telemetry score；
- 若 candidate 被 would_delay，则跳过该 warp并继续尝试下一个候选；
- 不要结束整个 scheduler cycle；
- 不要造成所有 warp 都被 delay；
- 必须有 allow/fallback 机制，避免 deadlock。

禁止：
- 大范围重构 scheduler；
- 实现真实 memory replay/re-execution；
- 改 ldst_unit memory request 语义；
- 改 DRAM / interconnect；
- 改 scoreboard 语义；
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
- rodinia_bfs
- strided_access

要求：
- feature_off 不变；
- policy_on 有真实 policy_prioritize 或 policy_delay signal；
- 无 hang/deadlock；
- 如果出现 extreme slowdown，停止扩大实验；
- 如果没有安全实现点，文档化 limitation，跳到 Phase 6 final report。

需要产出：
1. configs/hrl-repro/SM7_QV100_mascar_policy_on/
2. docs/papers/mascar_round_04_minimal_policy.md
3. experiments/paper-mascar/minimal_policy_check.csv
4. 更新 experiments/paper-mascar/config_matrix.csv
5. 更新 docs/papers/mascar_repro_plan.md
6. 更新 tools/paper_repro/papers/mascar.yaml
7. 更新 experiments/paper-mascar/round_state.yaml
8. 更新 CLAUDE.md
9. 追加 checkpoint log

成功标准：
- 编译通过；
- feature_off 不变；
- policy_on 有真实 signal；
- 无 hang/deadlock；
- 改动范围可解释。

Phase 4 通过后 commit：
git commit -m "mascar: add minimal scheduling policy"

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
- rodinia_bfs
- strided_access
- polybench_fdtd2d
- polybench_2dconv
- mutual_tiled
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
- memory_pressure_score
- saturation_event
- pitstop_event
- would_prioritize
- would_delay
- policy_prioritize
- policy_delay
- notes

需要产出：
1. docs/papers/mascar_round_05_focused_validation.md
2. experiments/paper-mascar/focused_validation.csv
3. 更新 experiments/paper-mascar/config_matrix.csv
4. 更新 docs/papers/mascar_repro_plan.md
5. 更新 experiments/paper-mascar/round_state.yaml
6. 更新 CLAUDE.md
7. 追加 checkpoint log

成功标准：
- 不改 src/；
- focused validation 完成；
- 结果可解释；
- 不跑 standard；
- 能判断 final report。

Phase 5 通过后 commit：
git commit -m "mascar: validate approximate scheduling policy on focused workloads"

============================================================
Phase 6：final report
============================================================

目标：
总结 Mascar approximate reproduction。
无论 Phase 4 是否成功，都要清楚写出当前复现状态。

不改 src。
不跑实验。

final report 结构：
1. Scope and Goal
2. Mascar Mechanism Recap
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
- 是否实现了真实 replay/re-execution；如果没有，为什么；
- 为什么结果可能不等同原论文；
- 是否建议停止 Mascar；
- 是否建议进入 cache policy 自研；
- 如果 Phase 4 停在 limitation，要清楚说明原因；
- 如果 Phase 4 成功，要说明 focused validation 是否支持大致预期。

需要产出：
1. docs/papers/mascar_final_reproduction_report.md
2. experiments/paper-mascar/final_summary.csv
3. 更新 docs/papers/mascar_repro_plan.md
4. 更新 tools/paper_repro/papers/mascar.yaml
5. 更新 experiments/paper-mascar/round_state.yaml
6. 更新 CLAUDE.md
7. 追加 checkpoint log

Phase 6 通过后 commit：
git commit -m "mascar: add final reproduction report"

============================================================
Checkpoint log 要求
============================================================

从 Phase 0 开始维护轻量 checkpoint log：

experiments/paper-mascar/mascar_one_shot_checkpoint_log.md

要求：
1. append-only，不要重写已有内容。
2. 每个 Phase 结束时追加一次。
3. 如果某个 Phase 执行接近或超过 10 分钟，先追加 checkpoint，再决定是否继续。
4. 进入 Phase 4 minimal mechanism 前必须追加 risk checkpoint。
5. 每次 checkpoint 控制在 8-12 行，不要写长篇叙述。
6. 不要把完整命令输出、大段 diff、完整 CSV 内容写进 log。
7. 如果任务中断，log 应能说明当前停在哪个 Phase、已改哪些文件、是否可以继续。

checkpoint 格式：

## Checkpoint: <timestamp or Phase name>

- Phase:
- 当前状态:
- 已完成:
- 正在做:
- 修改文件:
- 已运行验证:
- 当前风险:
- 是否建议继续:
- 下一步:

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

1. Mascar 复现进行到哪个 Phase。
2. 每个 Phase 是否完成。
3. 生成了哪些 commits。
4. 是否有未提交改动。
5. 是否存在 src 改动。
6. 是否出现 hang/deadlock/extreme slowdown。
7. 是否建议进入 GPT/Codex review。
8. 是否建议后续继续 Mascar，还是收束。
9. 不要 push，不要 tag，不要 /save-session。
```