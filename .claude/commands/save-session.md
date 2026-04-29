Please update the project CLAUDE.md for this repository.

Requirements:

1. Read the current CLAUDE.md if it exists.
2. Check the current project state:
   - pwd
   - git status --short || true
   - ls -la
   - test -f /workspace/repos/GPGPU-Sim-setup-notes.md && tail -80 /workspace/repos/GPGPU-Sim-setup-notes.md || true
   - test -f /workspace/repos/gpgpu-sim-build.log && tail -80 /workspace/repos/gpgpu-sim-build.log || true
   - test -f /workspace/repos/gpgpu-sim-run.log && tail -80 /workspace/repos/gpgpu-sim-run.log || true
3. Update CLAUDE.md with durable information from this session:
   - What changed
   - Commands that worked
   - Commands that failed
   - Current build/run status
   - Important environment variables
   - Known issues
   - Next steps
4. Do not include secrets, API keys, tokens, or unrelated chat.
5. Do not overwrite useful existing content. Merge, append, or revise carefully.
6. Keep it concise and useful for the next Claude Code session.
