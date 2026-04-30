# GPGPU-Sim Paper Reproduction and Idea Development Workflow

_Last updated: 2026-04-30 — Round Q: repro-infra-v0_

---

## 当前阶段定位

| 阶段 | 状态 |
|------|------|
| Workload bring-up | **完成** — 23 workloads ready，quick/standard/extended 分层已就绪 |
| Cache passive instrumentation | **完成** — branch `hrl/cache-instrumentation-v0`，tag `cache-inst-v0` |
| Paper reproduction workflow setup | **当前（Round Q）** — 只建框架，不做 cache policy 实验 |
| 第一篇 cache 论文复现 | 下一步（Round R+） |
| 自研 cache policy 实验 | 后置 — 等至少一篇论文流程跑通后再开 |

方向顺序：**先 cache，后 TLB/MMU**。

---

## 总体路线（Rounds）

| Round | 目标 | 限制 |
|-------|------|------|
| Q | repro-infra 框架文档、分支策略、模板 | 不改代码，不运行 workload |
| R | 选择第一篇 cache 论文，写 repro plan | 不改代码 |
| S | 第一篇论文 minimal implementation | 最小改动，feature flag default off |
| T | 将第一篇流程抽象成 `scripts/repro/*` | 提升脚本泛化能力 |
| U | 第二篇论文复现，验证脚本泛化 | 同 S |
| V | 创建 `hrl/idea/cache-policy-experiments-v0`，开始自研实验 | 不污染 paper branch |

---

## 分支策略

### Stable Points（不直接修改）

| 分支 / Tag | 说明 |
|-----------|------|
| `baseline-a4ce3fe` | 上游 a4ce3fe 对应的锚点 tag，永不修改 |
| `hrl/cache-instrumentation-v0` | Cache passive instrumentation 稳定分支 |
| `cache-inst-v0` | Cache instrumentation 稳定 tag |
| `hrl/repro-infra-v0` | 复现基础设施分支（本轮），framework 文档、模板 |

### Per-Paper Branches

| 模式 | 说明 |
|------|------|
| `hrl/paper/<paper-key>-repro-v0` | 单篇论文机制实现，忠实复现 |

每篇论文独立分支，互不干扰。创建方式：
```bash
git checkout hrl/repro-infra-v0
git checkout -b hrl/paper/<paper-key>-repro-v0
```

### Per-Idea Branches

| 模式 | 说明 |
|------|------|
| `hrl/idea/<idea-key>-v0` | 自研机制，独立于任何 paper branch |
| `hrl/idea/<idea-key>-from-<paper-key>-v0` | 基于某篇论文扩展的自研机制 |

创建方式：
```bash
git checkout hrl/repro-infra-v0
git checkout -b hrl/idea/<idea-key>-v0
```

### Integration Branches（只用于最终合并）

| 分支 | 说明 |
|------|------|
| `hrl/integration/cache-papers-v0` | 多篇 paper branch 选择性合并后验证 |
| `hrl/integration/cache-ideas-v0` | 多个 idea branch 选择性合并后验证 |
| `hrl/integration/cache-final-v0` | paper + idea 最终组合验证 |

---

## Paper Branch Policy

1. **每篇论文一个 paper branch**，base 为 `hrl/repro-infra-v0`（或当前最稳定的 infra tag）。
2. **忠实复现论文机制**，不混入自研创新。
3. **先写 repro plan**（`docs/papers/<paper-key>_repro_plan.md`），再改代码。
4. **feature flag default off**，必须通过 feature_off ≈ baseline 验证。
5. 成功后选择性合入 `hrl/integration/cache-papers-v0`，**不直接 merge 到 main 或其他 paper branch**。
6. 每篇论文独立 tag 节点，见 [Tag 策略](#tag-策略) 节。

---

## Idea Branch Policy

1. **每个自研机制一个 idea branch**，base 为 `hrl/repro-infra-v0`。
2. **不直接写进 paper branch**，idea 改动只在 idea branch 上进行。
3. 如果基于某篇论文扩展，用 `hrl/idea/<idea-key>-from-<paper-key>-v0` 命名，base 为该 paper branch 的稳定 tag。
4. **同样必须 feature flag default off**，同样必须跑 baseline / feature_off / feature_on。
5. 成熟后合入 `hrl/integration/cache-ideas-v0` 或 `hrl/integration/cache-final-v0`。

---

## Integration Branch Policy

1. Paper branch 和 idea branch **不互相直接 merge**。
2. 最终通过 integration 分支选择性合并，冲突只在 integration 分支解决。
3. 不污染原始 paper / idea 分支。
4. integration 分支本身只跑 standard + extended set 验证，不做新增开发。

---

## Feature Flag 规范

每个行为机制必须有 config 开关：

```
-gpgpu_enable_<paper_key>   0   # paper 机制开关，默认关闭
-gpgpu_enable_<idea_key>    0   # idea 机制开关，默认关闭
```

规则：
- **默认关闭（default 0）**。
- **feature_off** 状态应接近 baseline（数值差异来自 instrumentation overhead 本身，不应来自行为改变）。
- **feature_on** 才启用机制。
- 所有新增统计项必须以 `paper_<key>_` 或 `idea_<key>_` 前缀命名。

---

## 三组验证（必须）

每个 paper / idea 分支至少完成：

| 组 | 说明 | Pass 标准 |
|----|------|----------|
| **baseline** | 使用 `cache-inst-v0` tag 跑 quick set | 结果与原始 baseline 一致 |
| **feature_off** | 当前分支，`-gpgpu_enable_<key> 0` 跑 quick set | 结果应 ≈ baseline（同一 binary，开关关） |
| **feature_on** | 当前分支，`-gpgpu_enable_<key> 1` 跑 quick / standard set | 机制生效，观测预期变化 |

**baseline ≈ feature_off 是第一成功标准**，失败则 feature_off 本身有 bug，不可继续。

---

## Workload Set 使用规则

workload 仓库路径：`/workspace/repos/gpgpu-workloads`

| Set | 用途 | 频率 |
|-----|------|------|
| **quick** (7 workloads) | smoke regression，每次 feature 改动后必跑 | 频繁 |
| **standard** (13 workloads) | 正式实验主集合，paper/idea 结论依赖这组 | 每次 milestone |
| **extended** (全量) | 最终确认，不频繁跑 | 每篇论文最终确认前 |

常用命令：
```bash
cd /workspace/repos/gpgpu-workloads
bash scripts/run_workload_set.sh quick --dry-run          # 验证命令
bash scripts/run_workload_set.sh quick                    # 跑 quick set
bash scripts/run_workload_set.sh standard                 # 跑 standard set
python3 scripts/summarize_runs.py --csv > runs/latest_summary.csv
```

---

## Commit 节奏

### Paper Branch 建议节奏

```
1. paper notes / design mapping             # 读 repro plan，写 GPGPU-Sim 映射
2. config knobs only                        # 只加 config 参数，不改行为
3. instrumentation only                     # 只加统计，不改行为
4. minimal behavior implementation          # 最小行为实现（feature flag default off）
5. quick set pass                           # quick set feature_off ≈ baseline
6. standard set result                      # standard set feature_on 结果
```

### Idea Branch 建议节奏

```
1. idea motivation / design note            # 写 idea plan，记录 motivation
2. config knobs only                        # 只加 config 参数
3. instrumentation only                     # 只加统计
4. minimal behavior prototype               # 最小原型
5. quick set pass                           # feature_off ≈ baseline
6. standard set result                      # feature_on 结果
7. comparison with related paper branches   # 与相关 paper branch 对比（可选）
```

---

## Tag 策略

| Tag 模式 | 时机 |
|---------|------|
| `cache-inst-v0` | Cache instrumentation 完成（已创建） |
| `repro-infra-v0` | 本 Round Q 完成后创建 |
| `<paper-key>-plan-v0` | repro plan 写完，开始动代码前 |
| `<paper-key>-minimal-impl` | minimal implementation 完成 |
| `<paper-key>-quick-pass` | quick set feature_off ≈ baseline 确认 |
| `<paper-key>-standard-pass` | standard set feature_on 结果稳定 |
| `<idea-key>-prototype-v0` | idea prototype 完成 |
| `<idea-key>-quick-pass` | idea quick set pass |
| `<idea-key>-standard-pass` | idea standard set pass |

从 tag 恢复：
```bash
git checkout -b hrl/recover/cache-inst-v0 cache-inst-v0
```

---

## Worktree 建议

**不要在同一个工作目录的两个 tmux 中来回 checkout 不同分支。**

推荐用 git worktree 同时维护多个 paper / idea 分支：

```bash
# 为某篇论文创建独立 worktree
git worktree add /workspace/worktrees/gpgpu-paper-ccws hrl/paper/ccws-repro-v0

# 为自研机制创建独立 worktree
git worktree add /workspace/worktrees/gpgpu-idea-cache-policy hrl/idea/cache-policy-experiments-v0

# 列出所有 worktree
git worktree list

# 删除 worktree（分支保留）
git worktree remove /workspace/worktrees/gpgpu-paper-ccws
```

每个 worktree 有独立的工作目录，可以独立编译、运行，互不干扰。

---

## 回滚策略

```bash
# 从 tag 创建恢复分支
git checkout -b hrl/recover/<tag-name> <tag-name>

# 例：回到 cache-inst-v0
git checkout -b hrl/recover/cache-inst-v0 cache-inst-v0
```

---

## Git Hygiene

**不要提交以下内容：**
- 编译产物（`.o`, `.a`, `.so`, `libcudart.so`）
- PTX 文件
- 大型运行 log（`.log` > 1MB）
- checkpoint / simulator state 文件
- workload 可执行文件
- third-party suite 原始目录（Rodinia / PolyBench 源码）
- token / key / secret

每个 `experiments/paper-<key>/` 或 `experiments/idea-<key>/` 目录只提交 `run_notes.md`、`config_matrix.csv`、`result_manifest.csv`，**不提交 log**。

---

## 下一步

**Round R**：选择第一篇 cache 论文（推荐 cache bypassing / CCWS / DAWS 方向），填写 `docs/papers/<paper-key>_repro_plan.md`，**不立即改代码**。

候选论文方向（未最终确定）：
- CCWS（Coordinated Bypassing and Warp Throttling）
- DAWS（Demand-Aware Write-through Strategy）
- PCAL（Predicting Cache Amenability）
- Linebacker（LLC bypass for GPU）
