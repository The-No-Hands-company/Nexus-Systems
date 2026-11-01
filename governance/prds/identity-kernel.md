# Nexus Identity and Kernel PRD

**Owner:** Founder
**Maturity baseline:** L3
**Release target:** L4

## Product purpose

Identity and Kernel provide the shared platform guarantees behind every core
product: authentication, service discovery, health, routing intent, release
catalogue control, and cross-app trust boundaries.

## Verified current scope

- Nexus Auth provides ecosystem identity and role-aware access.
- Nexus Cloud coordinates tool registration, heartbeats, topology, routing,
  exposure, and approved catalog records.
- The dashboard catalog admits only healthy, released user applications.

## P1 requirements

- Cross-app audit logs for product and release decisions.
- Clear session-management and revocation behavior.
- Enforced release evidence linking Nit, staging, Founder approval, and
  changelog records.

## P2 requirements

- Signed service identities and stronger internal trust controls.
- Operator-facing health and release diagnostics.

## Release acceptance

- Auth and Cloud quality gates pass.
- Nit validates the changed ecosystem contracts.
- Staging verifies sign-in, session enforcement, catalog projection, and
  affected routing.
- Founder approves the release and records it in the changelog.
