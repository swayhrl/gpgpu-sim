# Tomorrow Queue Rehearsal — TOMORROW-RUN-PREP

**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0
**Round**: TOMORROW-RUN-PREP

---

## 测试了哪些 job

队列：`tools/paper_repro/job_queue.tomorrow.yaml.example`（4 jobs）

| Job ID | Stage | Risk | 预期 action | 实际 action | 通过 |
|--------|-------|------|------------|------------|------|
| `REPLACE_PAPER_KEY_reading` | `00_reading` | low | `continue` | `continue` | ✓ |
| `REPLACE_PAPER_KEY_noop` | `01_noop` | low | `continue` | `continue` | ✓ |
| `REPLACE_PAPER_KEY_telemetry` | `02_telemetry` | medium | `stop_for_review` | `stop_for_review` | ✓ |
| `REPLACE_PAPER_KEY_minimal_mechanism` | `04_minimal_mechanism` | high | `blocked_high_risk_stage` | `blocked_high_risk_stage` | ✓ |

---

## 生成文件验证

- `prompt.md` — 全部 4 个 job 均生成 ✓
- `gpt_review_packet.md` — 全部 4 个 job 均生成 ✓
- `next_action.yaml` — 全部 4 个 job 均生成 ✓
- `runs/paper_repro_queue/` — 被 `.gitignore` 正确忽略，不进入 git ✓

---

## Codex Reviewer Stub

```
python3 tools/paper_repro/codex_review_stub.py \
  --packet runs/paper_repro_queue/REPLACE_PAPER_KEY_reading/gpt_review_packet.md \
  --policy tools/paper_repro/reviewer_policy.example.yaml \
  --out runs/paper_repro_queue/REPLACE_PAPER_KEY_reading/codex_next_action.yaml \
  --dry-run
```

结果：`[dry-run] would write: ...codex_next_action.yaml` ✓

stub 默认不调用 Codex，安全输出 `stop_for_review`。

---

## 明天出门前如何使用

```bash
# 1. 确认工作树干净
git status --short   # 应无输出

# 2. 确认在正确 branch
git branch --show-current

# 3. 复制模板，填写真实论文
cp tools/paper_repro/job_queue.tomorrow.yaml.example \
   tools/paper_repro/job_queue.tomorrow.yaml
# 编辑：把所有 REPLACE_PAPER_KEY / REPLACE_BRANCH 替换为真实值

# 4. dry-run 验证
python3 tools/paper_repro/supervisor.py \
  --queue tools/paper_repro/job_queue.tomorrow.yaml \
  --dry-run

# 5. 如判断正确，正式运行
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.tomorrow.yaml

# 6. 对每个 action=continue 的 job：粘贴 prompt.md 到 Claude Code
# 7. 对每个 action=stop_for_review 的 job：发 gpt_review_packet.md 给 GPT
```

---

## 哪些情况必须停

| 情况 | 停止原因 |
|------|---------|
| `risk=high` 任何 job | supervisor 永远输出 `blocked_high_risk_stage` |
| `stage=minimal_mechanism` | 同上；代码层双重保护 |
| `stop_after_completion=true` | 执行后必须 review，不自动继续 |
| 工作树脏 | `blocked_dirty_repo` |
| `sim_cycle` 在 feature_off 改变 | 应立即停，报告 feature_off 破坏 |
| deadlock / timeout | 应立即停，输出 checkpoint summary |

---

## 当前系统仍不能做什么

- 不能自动执行 Claude Code（只生成 prompt，需人工粘贴）
- 不能自动 commit / tag / push
- 不能验证实验结果正确性（只检查 round_state 和 git 状态）
- 不能对 minimal_mechanism 输出 continue（永久禁止）
- 不能在 policy disabled 时调用 Codex
