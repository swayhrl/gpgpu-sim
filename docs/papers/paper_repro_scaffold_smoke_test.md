# Paper Reproduction Scaffold Smoke Test

**Round**: AUTO-0
**Date**: 2026-05-01
**Branch**: hrl/repro-auto-smoke-v0
**Tester**: Claude (automated)

---

## 测试目标

验证 `tools/paper_repro/` 半自动论文复现框架能否正常工作，为进入第二篇论文复现做准备。

---

## 测试结果

### 1. check_repo_clean.sh

```
bash tools/paper_repro/check_repo_clean.sh hrl/repro-auto-smoke-v0 paper-repro-scaffold-v0
```

**结果**：PASS
- OK: branch = hrl/repro-auto-smoke-v0
- OK: working tree clean
- OK: tag 'paper-repro-scaffold-v0' exists
- Pre-flight check PASSED.

### 2. make_round_prompt.py

测试了三个 stage：

| Stage | 输出行数 | paper config summary | stage instructions | round_state 要求 | 10-min checkpoint |
|-------|---------|---------------------|-------------------|-----------------|-------------------|
| 00_reading | ~60 行 | ✓ | ✓ | ✓ | ✓ |
| 05_focused_validation | ~80 行 | ✓ | ✓ | ✓ | ✓ |
| 07_final_report | 81 行 | ✓ | ✓ | ✓ | ✓ |

所有 stage 输出均包含：
- Paper config summary（paper_key, title, known_approximations, success_rules）
- Stage instructions（目标、允许、禁止、必须产出）
- round_state.yaml 必须字段列表
- Standard rules（不改 src/、不自动 commit/tag/push 等）
- Pre-flight check 命令

**结果**：PASS

### 3. stage_guard.sh

```
bash tools/paper_repro/stage_guard.sh --minutes 10 --note "AUTO-0 scaffold smoke test"
```

**结果**：PASS — 正确输出 checkpoint 模板，包含任务名称和时间限制。

### 4. Templates 长度检查

| Template | 行数 | 字节数 |
|----------|------|--------|
| 00_reading.md | 33 | 985 |
| 01_noop.md | 34 | 1154 |
| 02_telemetry.md | 33 | 1185 |
| 03_would_change.md | 31 | 1123 |
| 04_minimal_mechanism.md | 33 | 1160 |
| 05_focused_validation.md | 43 | 1575 |
| 06_standard_validation.md | 37 | 1281 |
| 07_final_report.md | 43 | 1322 |

**结论**：所有 template 均在合理范围内（最长 43 行 / 1575 字节），无过长、过细、过度约束问题。

### 5. ccws.yaml 作为样例

`tools/paper_repro/papers/ccws.yaml` 包含：
- paper_key, title, authors, venue, branch, base_branch, final_tag
- target_modules（3 个 src 文件）
- config_root, experiment_root, doc_root
- mechanism_chain（含 stage 映射）
- known_approximations, success_rules, workload_sets, reproduction_status

**结论**：结构完整，可作为第二篇论文 paper.yaml 的参考样例。

---

## 发现的小问题

### 问题 1：Pre-flight check 硬编码了 ccws 的 branch/tag

`make_round_prompt.py` 生成的 Pre-flight check 命令使用 `paper.yaml` 中的 `branch` 和 `final_tag`，对于新论文这是正确的。但如果新论文尚未有 `final_tag`（Stage 00-04 阶段），tag 参数会是空字符串，`check_repo_clean.sh` 会跳过 tag 检查——这是预期行为，无需修复。

### 问题 2：make_round_prompt.py 缺少 --list-stages 选项

目前没有办法快速列出所有可用 stage。用户需要手动查看 `templates/` 目录。影响较小，可在后续迭代中添加。

**当前无需修复的问题**：以上两点均不影响核心功能。

---

## 总结

| 检查项 | 状态 |
|--------|------|
| check_repo_clean.sh 可运行 | ✓ PASS |
| make_round_prompt.py 生成可用 prompt | ✓ PASS |
| stage_guard.sh 输出 checkpoint 规则 | ✓ PASS |
| templates 长度合理 | ✓ PASS |
| ccws.yaml 可作为新论文样例 | ✓ PASS |

**Round AUTO-0 结论**：scaffold 框架功能完整，可以进入 AUTO-1（选择第二篇论文，创建 paper.yaml）。

---

## 建议下一步（AUTO-1）

```bash
# 1. 复制 paper config 模板
cp tools/paper_repro/schemas/paper_config.example.yaml tools/paper_repro/papers/<key>.yaml

# 2. 填写 paper.yaml（paper_key, title, branch, mechanism_chain 等）

# 3. 生成第一个 stage prompt
python3 tools/paper_repro/make_round_prompt.py --paper <key> --stage 00_reading --round AUTO-1
```
