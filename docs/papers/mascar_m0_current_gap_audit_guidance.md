# Mascar M0 Detailed Guidance: Current Branch Audit, Gap Document, and Workspace Cleanup

## Stage position

This is M0 of the Mascar reproduction effort.

M0 is not an implementation round. It must not change simulator behavior. Its purpose is to freeze and understand the existing hrl/paper/mascar-repro-v0 branch, document the exact gap between the current approximate Mascar reproduction and the paper mechanism, and leave a clean review package for GPT.

M1 will add a passive L1 saturation probe.
M2 will implement paper-like EP/MP owner-warp scheduling.
M3 will attempt cache access re-execution.
M4 will validate and report results.

## Branch and repository

Repository:
  /workspace/repos/gpgpu-sim_distribution

Required branch:
  hrl/paper/mascar-repro-v0

Do not create a new branch.
Do not fetch upstream unless explicitly needed.
Use origin only for ordinary synchronization.

## Hard constraints

1. Do not modify simulator behavior in M0.
2. Do not modify src/gpgpu-sim files.
3. Do not modify config behavior.
4. Do not run long benchmark experiments.
5. Do not commit M0 result files.
6. Do not use git add . or git add -A.
7. Produce review files and a review pack.
8. Write actual start time, end time, and elapsed wall-clock seconds into postcheck.
9. If something is uncertain, mark it as unknown or needs M1 audit; do not invent claims.

Allowed changes:
  docs/papers/mascar_m0_gap_audit.md
  experiments/paper-mascar/m0_postcheck.md
  small audit text files under experiments/paper-mascar/

Expected output review pack:
  /workspace/tmp/mascar_m0_review_pack_YYYYMMDD_HHMMSS.tar.gz

## M0 tasks

### Task A: Workspace cleanup

Confirm current branch.

If a stray untracked file named exactly:
  tatus --short
exists, move it to:
  /workspace/tmp/gpgpusim_notes/

Do not delete potentially useful audit md files. Move local generated audit files out of the repo if they are untracked.

After cleanup, write the resulting status into postcheck.

### Task B: Branch ancestry audit

Collect these facts:

1. HEAD commit.
2. upstream tracking branch.
3. commits in:
   hrl/paper/daws-repro-v0..HEAD
4. commits in:
   paper-repro-supervisor-v0..HEAD
5. diff summary versus:
   hrl/paper/daws-repro-v0
   baseline-a4ce3fe

Use these only for documentation. Do not modify code.

### Task C: Current implementation audit

Inspect the current Mascar-related implementation.

At minimum inspect:
  docs/papers/
  experiments/
  configs/hrl-repro/
  tools/
  src/gpgpu-sim/shader.cc
  src/gpgpu-sim/shader.h
  src/gpgpu-sim/gpu-cache.cc
  src/gpgpu-sim/gpu-cache.h
  src/gpgpu-sim/gpu-sim.cc
  src/gpgpu-sim/gpu-sim.h
  src/gpgpu-sim/mem_fetch.cc
  src/gpgpu-sim/mem_fetch.h

Search terms:
  mascar
  MASCAR
  paper_mascar
  reexec
  re-exec
  owner
  owner_warp
  saturation
  m_mem_out
  scheduler_unit
  ldst_unit
  RESERVATION_FAIL
  MSHR
  miss_queue

For each current Mascar-like feature, record:
  feature name
  files and line references
  whether it is config-only, passive telemetry, approximate behavior, or paper-like behavior
  whether it is enabled by default
  whether it can affect baseline when disabled

### Task D: Paper mechanism target

In the gap audit document, summarize the paper target mechanism in your own words.

Required target mechanisms to cover:

1. EP mode:
   Equal priority mode when memory is not saturated.
   Memory-ready warps are favored to keep memory pipeline utilized.

2. MP mode:
   Memory access priority mode during memory saturation.
   One owner warp has priority/exclusivity for memory requests.

3. L1 saturation flag:
   The paper uses L1-side memory saturation/back-pressure detection based on L1 MSHR and miss queue pressure, not a generic scheduler-side proxy only.

4. WST and WRC:
   Warp Status Table has per-warp memory-operation and stall state.
   Warp Readiness Checker determines next memory operation, owner status, and scoreboard dependency.

5. Owner warp management:
   Owner warp keeps issuing memory requests until it reaches an instruction dependent on its own long-latency load, then relinquishes ownership.

6. MP prioritization:
   Compute-ready warps should have priority over memory-ready warps in MP mode to maximize memory/compute overlap.

7. Non-owner memory handling:
   Non-owner memory requests may be allowed to hit in L1, but should not be allowed to send misses to L2 during MP saturation.

8. Cache access re-execution queue:
   Requests blocked by memory back-pressure can be moved into a re-execution queue.
   LSU can later retry them.
   Non-owner miss should be NACKed/requeued.
   Owner miss can proceed if resources are available.

9. Ordering constraint:
   Only one memory instruction per warp should be in the re-execution queue at a time.

10. Validation:
   Required stats include EP cycles, MP cycles, owner switch count, owner issue count, non-owner blocked count, saturation cycles, re-exec enqueue, re-exec hit, re-exec NACK, re-exec retry, and deadlock guard events.

### Task E: Write gap audit document

Create:
  docs/papers/mascar_m0_gap_audit.md

Required sections:

1. Executive summary
   State clearly whether current branch is exact paper-like Mascar, approximate/proxy Mascar, or only telemetry.

2. Branch state and ancestry
   Include current branch, HEAD, ancestry relative to DAWS branch, and why this matters.

3. Current implementation summary
   Summarize current knobs, stats, scheduling changes, and reports.
   Use file:line evidence.

4. Paper mechanism checklist
   List the target Mascar mechanisms.

5. Gap matrix
   For each paper mechanism, provide:
     Paper requirement
     Current implementation
     Evidence file:line
     Gap severity: none, minor, medium, major, missing
     Recommended follow-up round: M1, M2, M3, M4, or later

6. No-op and baseline safety
   State whether current config defaults appear safe.
   Identify any risk that baseline behavior may be affected.

7. Recommended next rounds
   M1: passive L1 saturation probe
   M2: EP/MP owner scheduling without re-exec
   M3: re-execution queue
   M4: validation/report

8. Risks and unknowns
   Include cache API risk, LSU request representation risk, scoreboard/owner-release risk, deadlock risk, and validation limitations.

9. M0 signoff
   State exactly what M0 changed and did not change.

## Postcheck requirements

Create:
  experiments/paper-mascar/m0_postcheck.md

It must include:

1. start_iso, end_iso, elapsed_sec
2. current branch and HEAD
3. git status before and after
4. files changed
5. confirmation that src/gpgpu-sim was not modified
6. confirmation that configs were not modified
7. list of files GPT should review
8. review pack path
9. any warnings

Also create small audit helper files if useful:
  experiments/paper-mascar/m0_commits_after_daws.txt
  experiments/paper-mascar/m0_diff_name_status_vs_daws.txt
  experiments/paper-mascar/m0_symbol_grep.txt

Keep helper files reasonably short. Do not dump enormous logs.

## Review pack

Create a tar.gz at:
  /workspace/tmp/mascar_m0_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
  docs/papers/mascar_m0_gap_audit.md
  docs/papers/mascar_m0_current_gap_audit_guidance.md
  experiments/paper-mascar/m0_postcheck.md
  experiments/paper-mascar/m0_commits_after_daws.txt if present
  experiments/paper-mascar/m0_diff_name_status_vs_daws.txt if present
  experiments/paper-mascar/m0_symbol_grep.txt if present

Do not commit the M0 result files. Stop and report the review pack path.
