#!/usr/bin/env python3
"""
Codex CLI reviewer stub for L3-lite supervisor.

Default: dry-run / stub mode — never calls codex, returns deterministic stop_for_review.
Use --use-codex to attempt a real codex exec (read-only YAML review only).

Does NOT:
  - Modify any repository files
  - Commit / tag / push
  - Execute shell commands other than `codex` with a read-only prompt
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
PROMPT_TEMPLATE = Path(__file__).resolve().parent / "codex_review_prompt_template.md"

HIGH_RISK_STAGES = {"minimal_mechanism", "standard_validation", "behavior_change"}


def stub_action(reason: str = "Stub mode; API not enabled") -> dict:
    return {
        "source": "stub",
        "generated_at": datetime.now().isoformat(),
        "action": "stop_for_review",
        "risk_level": "unknown",
        "reason": reason,
        "recommended_next_stage": "none",
        "next_prompt_summary": "Human review required before proceeding.",
        "commit_recommended": False,
        "forbidden_actions_obeyed": True,
        "reviewer_notes": "Stub fallback — send gpt_review_packet.md to GPT manually.",
    }


def load_policy(policy_path: Path) -> dict:
    if not policy_path.exists():
        return {"enabled": False, "use_codex_exec": False}
    with open(policy_path) as f:
        return yaml.safe_load(f) or {}


def is_stage_high_risk(packet_text: str) -> bool:
    for stage in HIGH_RISK_STAGES:
        if stage in packet_text:
            return True
    return False


def run_codex_review(packet_path: Path, dry_run: bool) -> dict:
    """Call codex with a read-only review prompt. Returns parsed YAML dict or stub."""
    if not shutil.which("codex"):
        print("  codex not found in PATH; falling back to stub", file=sys.stderr)
        return stub_action("codex binary not found")

    template = PROMPT_TEMPLATE.read_text()
    packet_text = packet_path.read_text()

    # Safety check: never run codex on high-risk stages
    if is_stage_high_risk(packet_text):
        return stub_action("High-risk stage detected; codex review skipped for safety")

    prompt = (
        template
        + "\n\n## Review Packet\n\n"
        + packet_text
        + "\n\nOutput ONLY the YAML block described above."
    )

    if dry_run:
        print("  [dry-run] would call: codex exec --quiet <prompt>")
        return stub_action("dry-run; codex not called")

    # Write prompt to a temp file, pass via stdin to codex
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as f:
        f.write(prompt)
        prompt_path = f.name

    try:
        result = subprocess.run(
            ["codex", "--quiet", "--no-project-doc", "--full-auto",
             "-a", "read-only",
             "--approval-policy", "never",
             f"Read the file {prompt_path} and output ONLY the YAML review block."],
            capture_output=True, text=True, timeout=60, cwd=REPO_ROOT,
        )
        raw = result.stdout.strip()
        if result.returncode != 0 or not raw:
            print(f"  codex returned rc={result.returncode}; stderr: {result.stderr[:200]}",
                  file=sys.stderr)
            return stub_action(f"codex exec failed (rc={result.returncode})")

        # Parse first YAML block from output
        parsed = yaml.safe_load(raw)
        if not isinstance(parsed, dict):
            return stub_action("codex output not valid YAML dict")

        # Enforce invariants regardless of what codex said
        parsed["commit_recommended"] = False
        parsed["forbidden_actions_obeyed"] = True
        parsed["source"] = "codex_cli"
        parsed["generated_at"] = datetime.now().isoformat()

        # Never allow codex to continue high-risk stages
        stage_action = parsed.get("action", "stop_for_review")
        if parsed.get("risk_level", "low") == "high" and stage_action == "continue":
            parsed["action"] = "blocked"
            parsed["reason"] = "High-risk stage; codex output overridden to blocked"

        return parsed

    except subprocess.TimeoutExpired:
        return stub_action("codex exec timed out (>60s)")
    except Exception as e:
        return stub_action(f"codex exec exception: {e}")
    finally:
        Path(prompt_path).unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Codex CLI reviewer stub")
    parser.add_argument("--packet", required=True, help="Path to gpt_review_packet.md")
    parser.add_argument("--policy", default="tools/paper_repro/reviewer_policy.example.yaml",
                        help="Path to reviewer_policy yaml")
    parser.add_argument("--out", help="Write next_action.yaml to this path")
    parser.add_argument("--dry-run", action="store_true", help="Don't call codex")
    parser.add_argument("--use-codex", action="store_true",
                        help="Attempt real codex exec (requires policy.enabled=true)")
    args = parser.parse_args()

    packet_path = Path(args.packet)
    if not packet_path.exists():
        print(f"ERROR: packet not found: {packet_path}", file=sys.stderr)
        sys.exit(1)

    policy = load_policy(Path(args.policy))

    use_real_codex = (
        args.use_codex
        and policy.get("enabled", False)
        and policy.get("use_codex_exec", False)
        and not args.dry_run
    )

    if use_real_codex:
        print("  [codex_review_stub] using real codex exec")
        result = run_codex_review(packet_path, dry_run=False)
    elif args.use_codex and not args.dry_run:
        print("  [codex_review_stub] --use-codex given but policy disabled; using stub",
              file=sys.stderr)
        result = stub_action("policy.enabled=false or use_codex_exec=false")
    else:
        result = stub_action("dry-run / stub mode; codex not called" if args.dry_run
                             else "stub mode; use --use-codex to enable")

    output_yaml = yaml.dump(result, default_flow_style=False)

    if args.out:
        out_path = Path(args.out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if not args.dry_run:
            out_path.write_text(output_yaml)
            print(f"  wrote next_action to: {out_path}")
        else:
            print(f"  [dry-run] would write: {out_path}")
    else:
        print(output_yaml)


if __name__ == "__main__":
    main()
