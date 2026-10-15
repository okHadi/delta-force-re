# Delta Force RE

- Target Delta Force (1998) on Windows.
- Use REA for executable analysis.
- Keep original game files unchanged.
- Keep game audio muted during test runs. Keep system volume unchanged.
- Store game files, extracted assets, and full analysis output in `local/` or `.local/`.
- Link findings to the executable hash and code addresses.
- Mark inferred behavior and runtime checks clearly.
- Use short, simple text in code, docs, and replies.
- Use `emil-design-eng` for UI work.
- Test each complete iteration, then commit and push it. Group related changes to limit GitHub Actions runs.

- Run delegated workers through `scripts/opencode-worker` with Muse Spark 1.3 Contributor at `xhigh` effort on OpenCode Go. The lead agent reviews changes and test evidence, then commits and pushes.
