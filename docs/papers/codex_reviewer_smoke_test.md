# Codex Reviewer Smoke Test — AUTO-SUPERVISOR-2

**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0
**Round**: AUTO-SUPERVISOR-2

---

## Codex CLI 可用性

```
/root/.nvm/versions/node/v22.22.2/bin/codex
codex-cli 0.125.0
```

Codex CLI 可用。

---

## 测试内容

| 步骤 | 命令 | 结果 |
|------|------|------|
| stub dry-run | `codex_review_stub.py --dry-run` | 通过，输出 `would write next_action.yaml` |
| stub 完整运行 | `codex_review_stub.py`（无 --use-codex）| 通过，生成 `next_action.yaml`，action=stop_for_review |
| supervisor 回归 | smoke queue dry-run | 通过，3 job 判断不变 |
| supervisor --reviewer-stub | smoke queue --reviewer-stub | 通过，新参数不破坏结果 |

### next_action.yaml 输出（stub 模式）

```yaml
action: stop_for_review
commit_recommended: false
forbidden_actions_obeyed: true
reason: stub mode; use --use-codex to enable
recommended_next_stage: none
reviewer_notes: Stub fallback — send gpt_review_packet.md to GPT manually.
risk_level: unknown
source: stub
```

---

## 是否实际调用 codex exec

**没有**。原因：

1. `reviewer_policy.example.yaml` 默认 `enabled: false`、`use_codex_exec: false`
2. stub 模式已满足本轮 smoke test 目的（验证框架可运行）
3. 实际调用 codex 需要 `--use-codex` + policy enabled，且 packet 不含 high-risk stage

若要测试真实 codex exec，需要：
```bash
# 编辑 reviewer_policy.example.yaml：enabled=true, use_codex_exec=true
python3 tools/paper_repro/codex_review_stub.py \
  --packet runs/paper_repro_queue/codex_reviewer_smoke/gpt_review_packet.md \
  --policy tools/paper_repro/reviewer_policy.example.yaml \
  --out runs/paper_repro_queue/codex_reviewer_smoke/next_action.yaml \
  --use-codex
```

注意：smoke packet 含 `minimal_mechanism` 关键词，stub 会自动识别为 high-risk 并跳过 codex exec（即使 enabled）。

---

## 当前 Codex Reviewer 能做什么

- 读取 `gpt_review_packet.md`，输出 YAML review decision
- 分类 risk level，给出 recommended_next_stage 和 next_prompt_summary
- stub 模式下安全 fallback：永远 `action=stop_for_review`

## 当前不能做什么

- 不能自动修改任何文件
- 不能执行 shell 命令
- 不能 commit / tag / push
- 不能对 high-risk stage 输出 `continue`（被代码层强制覆盖）
- 不能在 policy disabled 时调用 codex（双重保护）

---

## 明天如何使用

**推荐流程**（文件式 review）：
```bash
# 1. 生成 review packet
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.tomorrow.yaml

# 2. stub 验证（可选）
python3 tools/paper_repro/codex_review_stub.py \
  --packet runs/paper_repro_queue/<job_id>/gpt_review_packet.md \
  --dry-run

# 3. 手动发 packet 给 GPT 或粘贴到 Claude Web
```

**若要用 Codex reviewer**（实验性）：
```bash
# 1. 先确认 packet 不含 high-risk stage
# 2. 编辑 reviewer_policy.example.yaml：enabled=true, use_codex_exec=true
# 3. 运行（仅 low-risk packet）
python3 tools/paper_repro/codex_review_stub.py \
  --packet ... --use-codex --out ...
# 4. 检查 next_action.yaml，commit_recommended 必须为 false
```
