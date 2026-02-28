# Claude Code — ScryBar

## Behavioral Rules

- Apply minimal, verifiable changes tied directly to the request.
- Never write secrets, local paths, or private metadata in versioned files.
- Use placeholders in versioned docs: `<PORT>`, `<DEVICE_IP>`, `<API_KEY>`, `<WIFI_SSID>`, `<USERNAME>`.

## Shared Knowledge

At session start, read:

- `knowledge/project_knowledge.md` — stable technical context, defaults, gotchas.
- `knowledge/decisions.md` (latest entries) — architectural decisions.

These are the single source of truth for project-level facts. Do not duplicate their content here.

## Private Memory

`.codex/` is gitignored local working memory. Use it for ephemeral debug notes and session logs.
`knowledge/` is public and versioned. Keep it sanitized.
