# Assistant Bootstrap Prompt (Cross-Provider)

Paste this at the start of any AI session for this project.

---

You are collaborating on this repository.

Use `knowledge/` as the public, shared memory layer for all assistants.

At session start:

1. Ensure `knowledge/` exists with exactly these files:
   - `knowledge/README.md`
   - `knowledge/assistant_bootstrap_prompt.md`
   - `knowledge/project_knowledge.md`
   - `knowledge/decisions.md`
2. If any file is missing, create it using the conventions in `knowledge/README.md`.
3. Read `knowledge/project_knowledge.md` and the latest entries in `knowledge/decisions.md`.
4. Scan project `README.md` and check git status before editing.

Operating rules:

- Keep changes minimal, verifiable, and directly tied to the request.
- Never write secrets in tracked files, logs, or commit messages.
- Never write local/private metadata in `knowledge/` (paths, IPs, ports, serial IDs, hostnames).
- Replace sensitive/runtime-specific values with placeholders (`<PROJECT_ROOT>`, `<DEVICE_IP>`, `<PORT>`, `<API_KEY>`).

How to write in `knowledge/`:

- `project_knowledge.md`: only stable technical facts, defaults, recurring gotchas.
- `decisions.md`: append short ADR-style entries with:
  - date
  - context
  - decision
  - impact/tradeoffs
- Keep entries concise; avoid raw terminal transcripts.

Public-by-design rationale:

- The repository remains understandable without provider-specific memory.
- Different assistants can continue work with the same shared context.
- Human contributors can review and improve project knowledge.

When finishing a task, report:

1. files changed
2. behavior impact
3. verification performed
4. residual risks / follow-ups

---
