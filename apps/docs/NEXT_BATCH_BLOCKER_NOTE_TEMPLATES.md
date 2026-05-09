# Next Batch Blocker Note Templates

Generated: 2026-05-08
Scope: Next best batch items 1 to 10

Batch order:
1. Nexus-Monitor
2. Nexus-Compliance
3. Nexus-AI-Hub
4. Nexus-Files
5. Nexus-Search
6. Nexus-IDE
7. Nexus-API
8. Nexus-Testing
9. Nexus-Forge
10. Nexus-Systems-API SDK and spec hardening

## Blocker Record Template

```md
### [APP] Blocker #[ID]

- Date: YYYY-MM-DD
- Batch position: N
- Severity: high | medium | low
- Type: contract | runtime | dependency | security | test | env
- Status: open | mitigated | closed

#### Blocked outcome
One sentence describing what cannot move forward.

#### Evidence
- failing endpoint, test, or command
- current observed behavior

#### Impact
- downstream apps affected
- work that can continue in parallel

#### Decision
- mitigation selected
- owner
- next checkpoint

#### Exit criteria
- objective condition proving unblock
```

## Logging Targets (Maturity-Aware)

1. Nexus-Monitor
- Primary log target: this file
- Reason: scaffold-level, no mature ops/risk doc yet

2. Nexus-Compliance
- Primary log target: this file
- Reason: scaffold-level

3. Nexus-AI-Hub
- Primary log target: this file
- Reason: scaffold-level

4. Nexus-Files
- Primary log target: this file
- Reason: scaffold-level

5. Nexus-Search
- Primary log target: this file
- Reason: scaffold-level

6. Nexus-IDE
- Primary log target: this file
- Reason: scaffold-level

7. Nexus-API
- Primary log target: this file
- Reason: scaffold-level

8. Nexus-Testing
- Primary log target: this file
- Reason: scaffold-level

9. Nexus-Forge
- Primary log target: Nexus-Forge/ROADMAP.md
- Mirror here section: Nexus-Forge blockers
- Reason: mature runtime with established roadmap and tests

10. Nexus-Systems-API
- Primary log target: Nexus-Systems-API/docs/README.md
- Mirror here section: Nexus-Systems-API blockers
- Reason: cloud-internal contract/spec stream with existing contract docs and tests

## Active Blockers

### Nexus-Monitor blockers
- follow-up (scoped): add invalid ingest payload smoke test
- follow-up (scoped): add cloud heartbeat registration integration hook

### Nexus-Compliance blockers
- none logged yet

### Nexus-AI-Hub blockers
- none logged yet

### Nexus-Files blockers
- none logged yet

### Nexus-Search blockers
- none logged yet

### Nexus-IDE blockers
- none logged yet

### Nexus-API blockers
- none logged yet

### Nexus-Testing blockers
- none logged yet

### Nexus-Forge blockers
- none logged yet

### Nexus-Systems-API blockers
- none logged yet
