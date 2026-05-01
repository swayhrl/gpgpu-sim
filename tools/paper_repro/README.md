# Paper Reproduction Automation Scaffold

本目录提供逐篇论文复现的半自动化脚手架。工具不会自动调用 Claude，也不会自动 commit/tag/push。所有操作由人工触发，工具只负责生成 prompt、检查 repo 状态、规范输出格式。

---

## 目录结构

```
tools/paper_repro/
  README.md                  本文件
  check_repo_clean.sh        repo 状态检查脚本
  make_round_prompt.py       stage prompt 生成器
  stage_guard.sh             10 分钟 checkpoint 提示辅助
  papers/
    ccws.yaml                CCWS 论文配置（样例）
    <paper_key>.yaml         新论文配置（按此格式创建）
  templates/
    00_reading.md            Stage 0: 论文阅读
    01_noop.md               Stage 1: no-op config + stats
    02_telemetry.md          Stage 2: instrumentation-only
    03_would_change.md       Stage 3: would-change telemetry
    04_minimal_mechanism.md  Stage 4: minimal mechanism
    05_focused_validation.md Stage 5: focused validation
    06_standard_validation.md Stage 6: standard validation
    07_final_report.md       Stage 7: final report
  schemas/
    round_state.example.yaml round_state.yaml 标准字段
    paper_config.example.yaml 新论文配置模板
    result_csv_fields.md     统一 CSV 字段建议
```

---

## 为新论文创建 paper.yaml

1. 复制 `schemas/paper_config.example.yaml` 到 `papers/<paper_key>.yaml`
2. 填写 `paper_key`、`title`、`branch`、`target_modules`、`mechanism_chain` 等字段
3. 参考 `papers/ccws.yaml` 作为已完成论文的样例

---

## 生成 stage prompt

```bash
python3 tools/paper_repro/make_round_prompt.py --paper ccws --stage 05_focused_validation
```

输出可直接复制给 Claude 的 prompt 文本。

---

## 检查 repo 状态

```bash
# 只检查 branch 和 clean status
bash tools/paper_repro/check_repo_clean.sh hrl/paper/ccws-repro-v0

# 同时检查 tag 是否存在
bash tools/paper_repro/check_repo_clean.sh hrl/paper/ccws-repro-v0 ccws-final-report
```

退出码 0 = 通过，非零 = 失败（附错误信息）。

---

## 使用 round_state.yaml 给 Claude 判断

每轮结束时，Claude 必须更新 `experiments/paper-<key>/round_state.yaml`。字段规范见 `schemas/round_state.example.yaml`。

下一轮开始时，将 round_state.yaml 内容粘贴到 prompt 中，让 Claude 了解上一轮结论和当前状态。

---

## 10 分钟 checkpoint 规则

每轮 prompt 中必须包含：

> 如果执行超过 10 分钟，请暂停，输出 checkpoint summary，不要继续扩展内容。

checkpoint summary 格式：
```
## Checkpoint Summary (N 分钟)
- 已完成：...
- 未完成：...
- 下一步：...
```

`stage_guard.sh` 提供标准提示模板，不控制进程。

---

## L3-lite 无人值守模式

`tools/paper_repro/` 现在支持 **L3-lite supervisor**：job queue + stop rules + review packet。

详见 **[README_unattended.md](README_unattended.md)**。

快速入门：
```bash
# 生成 prompt + review packet（dry-run）
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.example.yaml --dry-run

# 实际运行
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.example.yaml
```

---

## 工具限制

- **不自动调用 Claude**：所有 prompt 需人工复制粘贴
- **不自动 commit/tag/push**：所有 git 操作需人工确认
- **不控制 tmux/进程**：checkpoint 只是文本提示
- **不验证实验结果**：只生成 prompt 和检查 repo 状态
