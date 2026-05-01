#!/usr/bin/env python3
"""
L3-lite unattended paper reproduction supervisor.

Reads a job_queue.yaml, generates prompt.md and gpt_review_packet.md for each
job, and applies stop rules to decide whether each job can auto-continue or
needs human review.

Does NOT:
  - Call Claude / GPT API
  - Commit / tag / push
  - Control tmux or launch processes
  - Modify src/
"""
import argparse
import subprocess
import sys
from datetime import datetime
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = Path(__file__).resolve().parent
RUNS_DIR = REPO_ROOT / "runs" / "paper_repro_queue"

REVIEW_TEMPLATE = TOOLS_DIR / "review_packet_template.md"
MAKE_ROUND_PROMPT = TOOLS_DIR / "make_round_prompt.py"

# Stages that are high-risk and always require human sign-off
HIGH_RISK_STAGES = {"minimal_mechanism", "standard_validation", "behavior_change"}


def run(cmd: list[str], capture: bool = True) -> str:
    result = subprocess.run(cmd, capture_output=capture, text=True, cwd=REPO_ROOT)
    return result.stdout.strip() if capture else ""


def check_git_clean() -> tuple[bool, str]:
    status = run(["git", "status", "--short"])
    return (status == "", status)


def git_diff_stat() -> str:
    return run(["git", "diff", "--stat", "HEAD"])


def load_yaml(path: Path) -> dict | None:
    if not path.exists():
        return None
    with open(path) as f:
        return yaml.safe_load(f)


def generate_prompt(paper: str, stage: str, dry_run: bool) -> str:
    """Call make_round_prompt.py and return the generated prompt text."""
    cmd = [sys.executable, str(MAKE_ROUND_PROMPT), "--paper", paper, "--stage", stage]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    if result.returncode != 0:
        return f"[ERROR generating prompt]\n{result.stderr.strip()}"
    return result.stdout.strip()


def apply_stop_rules(job: dict, queue_cfg: dict, round_state: dict | None,
                     is_dirty: bool) -> tuple[str, str]:
    """Return (action, reason). action is one of:
      continue / stop_for_review / blocked_dirty_repo /
      blocked_missing_round_state / blocked_high_risk_stage
    """
    if is_dirty and queue_cfg.get("stop_on_dirty_repo", True):
        return "blocked_dirty_repo", "Working tree is dirty"

    stage = job.get("stage", "")
    stage_key = stage.split("_", 1)[-1] if "_" in stage else stage  # strip NN_ prefix

    if stage_key in HIGH_RISK_STAGES or stage_key in queue_cfg.get("require_human_review", []):
        return "blocked_high_risk_stage", f"Stage '{stage_key}' requires human review"

    if queue_cfg.get("stop_on_missing_round_state", True) and round_state is None:
        round_state_path = job.get("round_state_path", "<unknown>")
        if not Path(REPO_ROOT / round_state_path).exists():
            return "blocked_missing_round_state", f"round_state.yaml not found at {round_state_path}"

    if round_state is not None:
        rs_status = round_state.get("status", "")
        if rs_status not in ("done", "complete", "pending", ""):
            return "stop_for_review", f"round_state status='{rs_status}' (not done/complete/pending)"

    if stage_key in queue_cfg.get("allow_auto_continue", []):
        return "continue", "Stage is in allow_auto_continue list"

    return "stop_for_review", "Stage not in allow_auto_continue; defaulting to human review"


def write_output(job_id: str, prompt_text: str, review_text: str, dry_run: bool) -> Path:
    job_dir = RUNS_DIR / job_id
    job_dir.mkdir(parents=True, exist_ok=True)

    prompt_path = job_dir / "prompt.md"
    review_path = job_dir / "gpt_review_packet.md"

    if not dry_run:
        prompt_path.write_text(prompt_text)
        review_path.write_text(review_text)
    else:
        print(f"  [dry-run] would write {prompt_path}")
        print(f"  [dry-run] would write {review_path}")

    return job_dir


def build_review_packet(job: dict, prompt_path: str, round_state: dict | None,
                        git_status: str, diff_stat: str,
                        action: str, reason: str) -> str:
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    rs_summary = yaml.dump(round_state, default_flow_style=False) if round_state else "(not found)"

    questions = ""
    if action == "continue":
        questions = "1. Does the generated prompt look safe to execute?\n2. Any concerns about scope creep?"
    else:
        questions = (
            f"1. Stop rule fired: {reason}\n"
            "2. Is this a false positive? Should we override?\n"
            "3. What should the human do next?"
        )

    return f"""# GPT Review Packet — {job['id']}
Generated: {ts}

## Job Info
- paper: {job.get('paper')}
- stage: {job.get('stage')}
- description: {job.get('description', '')}

## Generated Prompt
Path: {prompt_path}

## Round State Summary
```yaml
{rs_summary}
```

## Git Status
```
{git_status or "(clean)"}
```

## Diff Stat
```
{diff_stat or "(no diff)"}
```

## Stop Rule Result
action: {action}
reason: {reason}

## Supervisor Recommendation
{"OK to auto-continue. Paste prompt.md into Claude Code." if action == "continue" else "STOP. Human review required before proceeding."}

## Questions for GPT
{questions}
"""


def process_job(job: dict, queue_cfg: dict, dry_run: bool) -> None:
    job_id = job["id"]
    paper = job.get("paper", "")
    stage = job.get("stage", "")

    print(f"\n=== Job: {job_id} ===")
    print(f"  paper={paper}  stage={stage}")

    is_dirty, git_status = check_git_clean()
    diff_stat = git_diff_stat()

    rs_path = REPO_ROOT / job.get("round_state_path", "MISSING")
    round_state = load_yaml(rs_path)

    action, reason = apply_stop_rules(job, queue_cfg, round_state, is_dirty)
    print(f"  stop_rule -> action={action}  reason={reason}")

    prompt_text = generate_prompt(paper, stage, dry_run)
    job_dir = write_output(job_id, prompt_text,
                           build_review_packet(job,
                                               str(RUNS_DIR / job_id / "prompt.md"),
                                               round_state, git_status, diff_stat,
                                               action, reason),
                           dry_run)

    next_action = {
        "job_id": job_id,
        "action": action,
        "reason": reason,
        "prompt_path": str(RUNS_DIR / job_id / "prompt.md"),
        "review_path": str(RUNS_DIR / job_id / "gpt_review_packet.md"),
        "generated_at": datetime.now().isoformat(),
    }
    na_path = RUNS_DIR / job_id / "next_action.yaml"
    if not dry_run:
        na_path.write_text(yaml.dump(next_action, default_flow_style=False))

    print(f"  output dir: {job_dir}")
    print(f"  next_action: {action}")


def main() -> None:
    parser = argparse.ArgumentParser(description="L3-lite paper repro supervisor")
    parser.add_argument("--queue", required=True, help="Path to job_queue.yaml")
    parser.add_argument("--dry-run", action="store_true", help="Don't write files")
    parser.add_argument("--job", help="Process only this job_id")
    args = parser.parse_args()

    queue_path = Path(args.queue)
    if not queue_path.exists():
        print(f"ERROR: queue file not found: {queue_path}", file=sys.stderr)
        sys.exit(1)

    queue = load_yaml(queue_path)
    if queue is None or "jobs" not in queue:
        print("ERROR: queue yaml missing 'jobs' key", file=sys.stderr)
        sys.exit(1)

    print(f"Queue: {queue.get('queue_name', '?')}  jobs={len(queue['jobs'])}  dry_run={args.dry_run}")

    jobs = queue["jobs"]
    if args.job:
        jobs = [j for j in jobs if j["id"] == args.job]
        if not jobs:
            print(f"ERROR: job '{args.job}' not found in queue", file=sys.stderr)
            sys.exit(1)

    for job in jobs:
        process_job(job, queue, args.dry_run)

    print("\nDone.")


if __name__ == "__main__":
    main()
