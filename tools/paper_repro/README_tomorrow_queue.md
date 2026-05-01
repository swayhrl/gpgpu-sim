# Tomorrow Queue — How to Run Unattended

## 准备步骤（今晚）

### 1. 选论文

决定明天复现哪篇论文，记下：
- `paper_key`（英文短标识，如 `new_paper`）
- 论文所在 branch 名称（如 `hrl/paper/new_paper-repro-v0`）
- 机制类型（warp_scheduler / cache_replacement / prefetch）

### 2. 复制 preplan 模板

```bash
cp tools/paper_repro/paper_preplan_template.yaml \
   tools/paper_repro/papers/new_paper_preplan.yaml
# 编辑所有 REPLACE_ME 字段
```

### 3. 创建 paper.yaml

```bash
cp tools/paper_repro/schemas/paper_config.example.yaml \
   tools/paper_repro/papers/new_paper.yaml
# 填写 paper_key, title, branch, target_modules, mechanism_chain 等字段
```

### 4. 填写明天的 job queue

```bash
cp tools/paper_repro/job_queue.tomorrow.template.yaml \
   tools/paper_repro/job_queue.tomorrow.yaml
# 只保留明天要跑的 stage，删掉 placeholder jobs
# 填写 paper, expected_branch
```

示例（只跑 low-risk stages）：

```yaml
jobs:
  - id: new_paper_reading
    paper: new_paper
    stage: 00_reading
    expected_branch: hrl/paper/new_paper-repro-v0
    risk: low
    allow_src_change: false
    stop_after_completion: false
    requires_gpt_review: false
    max_minutes: 10
    ...
```

---

## 明天出门前（5 分钟）

```bash
# 1. 确认工作树干净
git status --short   # 应无输出

# 2. 生成所有 job 的 prompt + review packet
bash tools/paper_repro/run_queue.sh tools/paper_repro/job_queue.tomorrow.yaml

# 3. 查看判断结果
ls runs/paper_repro_queue/

# 4. 对 action=continue 的 job：粘贴 prompt.md 到 Claude Code 执行
cat runs/paper_repro_queue/<job_id>/prompt.md

# 5. 对 action=blocked_* 的 job：先发 gpt_review_packet.md 给 GPT 判断
cat runs/paper_repro_queue/<job_id>/gpt_review_packet.md
```

---

## 哪些 stage 可无人值守

| Stage | 风险 | 可无人值守 |
|-------|------|-----------|
| `00_reading` | low | ✓ |
| `01_noop` | low | ✓ |
| `02_telemetry` | low | ✓ |
| `03_would_change` | medium | 可执行，完成后须 review |
| `07_final_report` | low | ✓ |
| `workload_audit` | low | ✓ |

## 哪些 stage 必须停下

| Stage | 原因 |
|-------|------|
| `04_minimal_mechanism` | 首次行为改动，有 deadlock 风险 |
| `05_focused_validation` | 需要判断 cycle 方向是否符合论文 |
| `06_standard_validation` | 全量 workload，风险高 |
| 任何 `behavior_change` | 调度 / cache 语义改变 |

---

## 如何把 review packet 发给 GPT

1. 打开 `runs/paper_repro_queue/<job_id>/gpt_review_packet.md`
2. 复制全文，粘贴到 ChatGPT 或 Claude Web
3. 提问：
   > "根据这个 review packet，这个 paper reproduction stage 安全吗？有什么风险？"
4. 根据回答决定是否继续

---

## 安全规则

- **不允许**自动 commit / tag / push
- **不允许** high-risk stage 自动连续推进
- `minimal_mechanism` 和 `standard_validation` supervisor 永远输出 `blocked_high_risk_stage`
- 脏工作树时全部 job 被阻塞

---

## GPT API review stub

`tools/paper_repro/gpt_review_stub.py` 已创建，但默认不调用 API：

```bash
python3 tools/paper_repro/gpt_review_stub.py \
  --review-packet runs/paper_repro_queue/<job_id>/gpt_review_packet.md
# 输出 action: stop_for_review（stub 模式，安全）
```

若要启用真实 API（将来）：
1. 设置 `OPENAI_API_KEY`
2. 将 `gpt_supervisor_policy.example.yaml` 中 `enabled` 改为 `true`
3. API 仍只允许 review / prompt generation，禁止 commit / push / 执行危险动作
