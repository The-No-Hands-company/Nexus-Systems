# Nexus Release Policy

## Development layers

Every item in the backlog belongs to exactly one layer:

1. **Critical** — security, data loss, authentication, availability, or other
   platform-breaking faults. Release as a hotfix after the focused gate.
2. **Product feature** — user-facing capability promised by a PRD.
3. **Quality improvement** — performance, accessibility, resilience,
   responsiveness, or usability work.
4. **Future vision** — larger roadmap work that is not yet a release commitment.

## Release channels

```text
Development -> Testing -> Staging -> Production -> Changelog
```

| Channel | Purpose | Entry rule |
|---|---|---|
| Development | Active implementation | PRD scope and acceptance criteria exist |
| Testing | Automated and focused manual verification | App quality gate passes |
| Staging | Production-like validation | Nit release gate passes |
| Production | User-facing release | Founder approves product readiness |

## Required production evidence

Every production release must record:

- linked PRD and backlog item;
- test command and result for every changed active app;
- Nit release-gate result;
- staging smoke-check result for the affected user journey;
- release decision by the Founder;
- rollback path for changes that affect data, authentication, routing, or
  public delivery;
- `CHANGELOG.md` entry.

Nit is the mandatory ecosystem technical gate. It does not replace product
acceptance, staging validation, or Founder approval.

## Exceptions and hotfixes

Critical work may bypass normal scheduling, but not verification. The Founder
may approve an exception only when the release record states the reason,
remaining risk, and follow-up work. The changelog must identify it as a
hotfix.

## Cohort protection

No new application may enter development until the initial core cohort meets
its maturity targets in `MATURITY_MATRIX.md`. An exception requires a Founder
decision recorded in the backlog and changelog.
