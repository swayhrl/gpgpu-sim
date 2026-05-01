#!/usr/bin/env python3
"""
GPT review stub for L3-lite supervisor.

In stub mode (default), returns a deterministic stop_for_review action without
calling any external API. This is safe to commit and run in CI.

To enable real API review:
  1. Set OPENAI_API_KEY in environment.
  2. Set enabled=true in gpt_supervisor_policy.example.yaml.
  3. This stub will still NOT call the API until those two conditions are met.

Does NOT:
  - Execute shell commands
  - Modify the repository
  - Commit / tag / push
"""
import argparse
import os
import sys
from datetime import datetime
from pathlib import Path

import yaml


def stub_review(review_packet_path: Path) -> dict:
    """Return a deterministic stub decision without calling any API."""
    return {
        "source": "stub",
        "generated_at": datetime.now().isoformat(),
        "review_packet": str(review_packet_path),
        "action": "stop_for_review",
        "reason": "API not enabled; stub always returns stop_for_review for safety",
        "recommendation": (
            "Read gpt_review_packet.md manually and decide whether to proceed. "
            "To enable API review, set OPENAI_API_KEY and enabled=true in "
            "gpt_supervisor_policy.example.yaml."
        ),
        "questions": [],
    }


def load_policy(policy_path: Path) -> dict:
    if not policy_path.exists():
        return {"enabled": False}
    with open(policy_path) as f:
        return yaml.safe_load(f) or {"enabled": False}


def is_api_enabled(policy: dict) -> bool:
    if not policy.get("enabled", False):
        return False
    if not os.environ.get("OPENAI_API_KEY"):
        return False
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="GPT review stub for L3-lite supervisor")
    parser.add_argument("--review-packet", required=True, help="Path to gpt_review_packet.md")
    parser.add_argument("--policy", default="tools/paper_repro/gpt_supervisor_policy.example.yaml",
                        help="Path to gpt_supervisor_policy yaml")
    parser.add_argument("--output", help="Write next_action.yaml to this path")
    args = parser.parse_args()

    review_path = Path(args.review_packet)
    if not review_path.exists():
        print(f"ERROR: review packet not found: {review_path}", file=sys.stderr)
        sys.exit(1)

    policy_path = Path(args.policy)
    policy = load_policy(policy_path)

    if is_api_enabled(policy):
        # Placeholder: real API call would go here.
        # For now, fall through to stub even if somehow enabled.
        print("NOTE: API enabled in policy but real call not implemented; using stub.",
              file=sys.stderr)

    result = stub_review(review_path)

    output_yaml = yaml.dump(result, default_flow_style=False)
    if args.output:
        Path(args.output).write_text(output_yaml)
        print(f"Wrote next_action to: {args.output}")
    else:
        print(output_yaml)


if __name__ == "__main__":
    main()
