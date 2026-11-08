# Nexus v1.0 Production Maturity Radar

> **Development Helper Tool (DHT)** for auditing, scoring, and tracking every Nexus application on its path to a fully featured **v1.0 Production Grade** state.

---

## What this tool is

As the Nexus ecosystem expands, keeping track of what is truly feature-complete versus what is merely running a placeholder or beta shell is essential. 

The **v1.0 Production Maturity Radar** provides an objective, measured index across **6 Core Pillars**:
1. **Data & Persistence Layer (20 pts):** Real domain models, schemas, and persistence vs ghost scaffold CRUD.
2. **UI & UX Completeness (25 pts):** Interactive frontends adhering to Void/Slate design tokens, error boundaries, responsive layouts.
3. **Core Domain Features (25 pts):** App-specific feature checklist (e.g., Mail: WYSIWYG, attachments, search; Calendar: week views, .ics sync; Chat: threads, reactions; etc.).
4. **API & Protocol Conformance (10 pts):** REST/WebSocket endpoints, session verification, error standardization.
5. **Quality Gates & Tests (10 pts):** `check.sh` quality gate passes, unit tests, integration tests.
6. **Platform & Delivery (10 pts):** Live port running under `deploy.sh`, Caddy reverse-proxy route, HTTP 200 health check.

### Stages:
- 🔴 **Scaffold (0-29%):** Skeleton files only, no real domain logic.
- 🟠 **Alpha (30-59%):** Basic backend or frontend working; major features missing.
- 🟡 **Beta (60-79%):** Functional and partially usable; missing key QoL, search, attachments, or edge-case handling.
- 🟢 **Release Candidate (80-94%):** Feature-complete and integrated; needs polish and battle-testing.
- 🌟 **v1.0 Production Grade (95-100%):** Complete, delightful, resilient, zero regressions.

---

## How to Invoke

### 1. Terminal Scoreboard (CLI)
```bash
# Print ecosystem summary table
bun run dhts/nexus-v1-radar/scan.ts

# Inspect deep-dive checklist for a specific app
bun run dhts/nexus-v1-radar/scan.ts --app nexus-email
bun run dhts/nexus-v1-radar/scan.ts --app nexus-calendar
bun run dhts/nexus-v1-radar/scan.ts --app nexus-docs

# Generate updated markdown register in docs/V1-MATURITY-RADAR.md
bun run dhts/nexus-v1-radar/scan.ts --markdown

# Output pure JSON for CI or automation
bun run dhts/nexus-v1-radar/scan.ts --json
```

### 2. Interactive Web Dashboard
```bash
# Start the web dashboard on port 3200
bun run dhts/nexus-v1-radar/server.ts
```
Then open `http://localhost:3200` in your browser.
- Live progress bars and stage badges.
- Search and category filters.
- Click any app card to view the exact feature checklist (Done vs Missing) and the recommended roadmap to reach v1.0.
