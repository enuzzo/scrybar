# Knowledge Base

Shared, provider-agnostic project knowledge for assistants and humans.

This folder is public and versioned by design.

## Why Public

- Makes context portable across assistants (Codex, Claude, Gemini, etc.).
- Keeps project practices visible to collaborators.
- Reduces assistant lock-in to provider-specific memory systems.

## Canonical Files

- `README.md`: policy and structure for this folder.
- `assistant_bootstrap_prompt.md`: prompt to paste at session start.
- `project_knowledge.md`: stable technical context and defaults.
- `decisions.md`: concise architectural/operational decisions (ADR-lite).

## Data Policy (Mandatory)

Never store in `knowledge/`:

- secrets, tokens, passwords, API keys
- local machine paths (`/Users/...`, `C:\\Users\\...`)
- LAN/private infra details (IPs, hostnames, SSIDs, MACs)
- personal device identifiers (serial ports, USB IDs)
- raw terminal dumps with local/private metadata

Use placeholders:

- `<PROJECT_ROOT>`
- `<PORT>`
- `<DEVICE_IP>`
- `<WIFI_SSID>`
- `<API_KEY>`

## Writing Rules

- Keep content reusable, short, and verifiable.
- Move ephemeral debugging details to private local memory (not versioned).
- Update `project_knowledge.md` only for stable facts.
- Append decisions to `decisions.md` with date, context, choice, impact.
