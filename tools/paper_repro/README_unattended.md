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

---

## L3-lite 文件式 Review（推荐方式）

明天实际使用仍以**文件式 review** 为主：

1. 运行 `run_queue.sh` 生成 review packet
2. 手动读取 `gpt_review_packet.md`
3. 粘贴到 ChatGPT / Claude Web 获取建议
4. 自行决定是否继续

---

## GPT API Review Stub（将来扩展）

`tools/paper_repro/gpt_review_stub.py` 是 API review 的 stub：

```bash
# 当前默认：stub 模式，不调用 API，返回 stop_for_review
python3 tools/paper_repro/gpt_review_stub.py \
  --review-packet runs/paper_repro_queue/<job_id>/gpt_review_packet.md
```

若要启用真实 API：
1. 设置 `OPENAI_API_KEY`
2. 编辑 `gpt_supervisor_policy.example.yaml`，将 `enabled: true`
3. API **只允许** review / prompt generation
4. API **禁止** commit / push / 执行 shell / 修改 src/

权限策略见 `gpt_supervisor_policy.example.yaml`。风险分级见 `risk_policy.md`。

---

## 新字段支持（supervisor v2）

`job_queue.yaml` 现在支持以下 job-level 字段：

| 字段 | 说明 | 默认值 |
|------|------|--------|
| `risk` | `low` / `medium` / `high` | `low` |
| `stop_after_completion` | 完成后必须 stop_for_review | `false` |
| `requires_gpt_review` | 需要 GPT review 才能继续 | `false` |
| `allow_src_change` | `false` 时 src/ diff 触发 blocked | `true` |

`risk=high` 的 job 永远输出 `blocked_high_risk_stage`，无论 stage key 是什么。

