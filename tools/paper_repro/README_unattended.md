# L3-lite Unattended Paper Reproduction Supervisor

## 什么是 L3-lite？

L3-lite 是介于"全手动"（L1）和"全自动"（L4）之间的一个轻量级监督层：

| 级别 | 描述 |
|------|------|
| L1   | 全手动：每步都需要人类审查和执行 |
| L2   | 半自动：脚手架生成 prompt，人类逐步执行 |
| **L3-lite** | **job queue + stop rules：低风险 stage 生成可执行 prompt，人类离开前批准，高风险 stage 自动暂停** |
| L4   | 全自动：Claude 自主循环，无人值守（不在本仓库范围内） |

---

## 能做什么

- **读取 job_queue.yaml**，批量生成每个 job 的 `prompt.md`
- **根据 stop rules 判断**每个 job 是否可以自动继续（`continue`）还是需要人工介入（`stop_for_review`）
- **生成 gpt_review_packet.md**，包含 round_state、git status、diff stat、建议
- **检查 repo 状态**：脏工作树、缺失 round_state、意外 src/ 改动

## 不能做什么

- **不自动调用 Claude Code**（只生成 prompt，需要人类粘贴执行）
- **不自动 commit / tag / push**
- **不调用 GPT API**（只生成可粘贴的 review packet）
- **不控制 tmux / 终端进程**
- **不执行 minimal_mechanism 或 standard_validation 阶段**（这两个总是停下来等人类）
- **不修改 src/**

---

## 明天出门前：快速使用指南

**前提**：已写好 `job_queue.yaml`，工作树干净。

```bash
# 1. 生成所有 job 的 prompt + review packet
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.example.yaml

# 2. 查看每个 job 的判断结果
cat runs/paper_repro_queue/<job_id>/gpt_review_packet.md

# 3. 对 action=continue 的 job：直接粘贴 prompt.md 内容到 Claude Code 执行
cat runs/paper_repro_queue/<job_id>/prompt.md

# 4. 对 action=stop_for_review 的 job：先把 gpt_review_packet.md 发给 GPT 判断
```

**dry-run 测试**（不写文件）：

```bash
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.example.yaml --dry-run
```

---

## 如何把 review packet 发给 GPT

1. 打开 `runs/paper_repro_queue/<job_id>/gpt_review_packet.md`
2. 复制全文
3. 在 ChatGPT / Claude Web 中粘贴，加提问：
   > "请根据这个 review packet 判断这个 job 是否安全执行，有什么风险？"
4. 根据 GPT 回答决定是否继续

---

## 安全边界

- **必须人工确认**：`minimal_mechanism`、`standard_validation`、任何 `behavior_change`
- **自动允许**：`reading`、`noop`、`telemetry`、`would_change`、`final_report`、`workload_audit`
- **脏工作树时全部阻塞**：supervisor 不会在工作树脏的情况下允许任何 job 继续
- **不跨 paper branch 混入 src 改动**：由 `stop_on_src_diff` 保护

详见 `stop_rules.md`。

---

## 文件结构

```
tools/paper_repro/
  supervisor.py               # 主程序
  run_queue.sh                # 入口脚本
  job_queue.example.yaml      # job queue 示例
  stop_rules.md               # stop rules 规范
  review_packet_template.md   # review packet 模板（文档用）
  queue_state.example.yaml    # queue 运行状态示例
  next_action.example.yaml    # next_action 输出示例
  README_unattended.md        # 本文件

runs/paper_repro_queue/
  <job_id>/
    prompt.md                 # 生成的 Claude 执行 prompt
    gpt_review_packet.md      # 发给 GPT 的 review packet
    next_action.yaml          # 机器可读的 action 决策
```
