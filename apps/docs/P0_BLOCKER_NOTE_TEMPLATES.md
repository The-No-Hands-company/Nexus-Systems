# P0 Blocker Note Templates

Generated: 2026-05-08
Scope: Locked queue apps 1 to 10
Purpose: Standard blocker notes without forcing new docs into every app folder

Use this file as the source of truth for blocker logging during Wave P0.
Rule: if an app already has a mature planning/risk doc, log blockers there and copy the short record into this file.

## Blocker Record Template

Copy this block when a blocker appears:

```md
### [APP] Blocker #[ID]

- Date: YYYY-MM-DD
- Queue position: N
- Severity: high | medium | low
- Type: contract | runtime | security | dependency | test | environment
- Status: open | mitigated | closed

#### Blocked outcome
One sentence on what cannot move forward.

#### Evidence
- endpoint/path/test that fails
- command used
- short output summary

#### Impact
- immediate downstream app(s) affected
- what can continue in parallel

#### Decision
- chosen mitigation path
- owner
- next checkpoint date

#### Exit criteria
- measurable condition proving unblock
```

## App-Specific Logging Targets (Maturity-Aware)

1. Nexus-Cloud
- Primary log target: Nexus-Cloud/docs/implementation-plan.md
- Mirror here section: Nexus-Cloud blockers
- Reason: mature planning doc already tracks blockers and dependencies

2. Nexus-Auth
- Primary log target: this file (until a dedicated operations log exists)
- Mirror optional: Nexus-Auth/docs/README.md
- Reason: MVP shell is in progress with lightweight docs

3. Nexus-Vault
- Primary log target: this file
- Mirror optional: Nexus-Vault/README.md security or ops notes
- Reason: runtime is mature; blocker log should stay lean

4. Nexus-Guardian
- Primary log target: this file
- Reason: scaffold stage; avoid creating extra docs in app folder until MVP runtime exists

5. Nexus-Tunnel
- Primary log target: this file
- Reason: scaffold stage; avoid document sprawl before implementation

6. Nexus-Edge
- Primary log target: this file
- Reason: scaffold stage; avoid document sprawl before implementation

7. Nexus-Computer
- Primary log target: Nexus-Computer/docs/NEXUS_GAP_AUDIT_2026-04-14.md
- Mirror here section: Nexus-Computer blockers
- Reason: existing audit doc already acts as a gap/blocker surface

8. Nexus-AI
- Primary log target: Nexus-AI/docs/ROADMAP.md
- Mirror optional: Nexus-AI/docs/EXECUTION_SUMMARY.md
- Reason: mature roadmap and execution docs already exist

9. Nexus-Network
- Primary log target: this file
- Reason: compact docs footprint; keep blocker records centralized for P0

10. Nexus-Hosting
- Primary log target: Nexus-Hosting/ROADMAP.md
- Mirror optional: Nexus-Hosting/docs/HONEST_ASSESSMENT.md
- Reason: highly mature project with existing status tracking

## Active Blockers by App

### Nexus-Cloud blockers
- none logged yet

### Nexus-Auth blockers
- none logged yet

### Nexus-Vault blockers
- none logged yet

### Nexus-Guardian blockers
- none logged yet

### Nexus-Tunnel blockers
- none logged yet

### Nexus-Edge blockers
- none logged yet

### Nexus-Computer blockers
- none logged yet

### Nexus-AI blockers
- none logged yet

### Nexus-Network blockers
- none logged yet

### Nexus-Hosting blockers
- none logged yet
