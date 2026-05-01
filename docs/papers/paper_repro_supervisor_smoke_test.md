# Paper Repro Supervisor Smoke Test — AUTO-SUPERVISOR-1

**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0
**Round**: AUTO-SUPERVISOR-1

---

## 测试队列

`tools/paper_repro/job_queue.smoke.yaml`（3 个 jobs）

| Job ID | Paper | Stage | 预期 action |
|--------|-------|-------|-------------|
| `safe_reading` | daws | `00_reading` | `continue` |
| `safe_final_report` | daws | `07_final_report` | `continue` |
| `blocked_minimal_mechanism` | daws | `04_minimal_mechanism` | `blocked_high_risk_stage` |

---

## 测试结果

### supervisor.py dry-run（全队列）

```
Queue: smoke_test  jobs=3  dry_run=True

=== Job: safe_reading ===
  stop_rule -> action=continue  reason=Stage is in allow_auto_continue list

=== Job: safe_final_report ===
  stop_rule -> action=continue  reason=Stage is in allow_auto_continue list

=== Job: blocked_minimal_mechanism ===
  stop_rule -> action=blocked_high_risk_stage  reason=Stage 'minimal_mechanism' requires human review
```

**结论**：stop rules 判断全部正确。

### supervisor.py --job 单 job 测试

- `--job safe_reading --dry-run` → `continue` ✓
- `--job blocked_minimal_mechanism --dry-run` → `blocked_high_risk_stage` ✓

### run_queue.sh 完整运行

生成文件：

```
runs/paper_repro_queue/
  safe_reading/            prompt.md  gpt_review_packet.md  next_action.yaml
  safe_final_report/       prompt.md  gpt_review_packet.md  next_action.yaml
  blocked_minimal_mechanism/  prompt.md  gpt_review_packet.md  next_action.yaml
```

所有 3 个 job 均正确生成全部 3 个输出文件。

### .gitignore 验证

`runs/paper_repro_queue/` 已在 `.gitignore` 中，运行后 `git status --short` 干净（只有新建 job_queue.smoke.yaml 为 untracked）。

---

## Stop Rules 验证

| 验证点 | 结果 |
|--------|------|
| `reading` → `continue` | ✓ |
| `final_report` → `continue` | ✓ |
| `minimal_mechanism` → `blocked_high_risk_stage` | ✓ |
| supervisor 不 commit/tag/push | ✓（代码中无 git 写操作） |
| supervisor 不调用 Claude / GPT API | ✓（代码中无外部 API 调用） |
| `runs/` 不进 Git | ✓ |
| `src/` 未修改 | ✓ |

---

## 当前 L3-lite 能力

- 读取 `job_queue.yaml`，批量或单 job 生成 prompt.md
- 根据 stop rules 正确区分 safe（`continue`）和 high-risk（`blocked_high_risk_stage`）stage
- 检查 `round_state.yaml` 存在性和 status 字段
- 检查 git 工作树是否干净（`blocked_dirty_repo`）
- 生成 `gpt_review_packet.md`（包含 round_state 摘要、git status、stop rule 决策、GPT 问题）
- 生成 `next_action.yaml`（机器可读决策）
- dry-run 模式（`--dry-run`）不写文件
- 单 job 模式（`--job <id>`）

## 当前 L3-lite 不能做

- 不自动调用 Claude Code 执行 prompt
- 不自动 commit / tag / push
- 不控制 tmux / 终端进程
- 不调用 GPT API（review packet 需手动发给 GPT）
- 不自动进入 `minimal_mechanism` 或 `standard_validation` 阶段

---

## 明天出门前使用建议

1. 确保工作树干净（`git status --short` 无输出）
2. 准备 job queue yaml（参考 `job_queue.smoke.yaml` 或 `job_queue.example.yaml`）
3. 运行 supervisor 生成所有 safe job 的 prompt：
   ```bash
   bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.smoke.yaml
   ```
4. 对每个 `action=continue` 的 job，粘贴 `prompt.md` 内容到 Claude Code 执行
5. 对每个 `action=blocked_*` 的 job，先把 `gpt_review_packet.md` 发给 GPT 判断是否安全
6. 回来后检查 `experiments/paper-*/round_state.yaml` 确认 status=complete
7. 提交前确认 `src/` 无意外改动

**关键规则**：`minimal_mechanism`、`standard_validation`、任何 `behavior_change` 阶段
supervisor 会自动拦截，不会生成 `continue` action。
