# Dependency issue triage — 2026-10-18

The five open Stack Control issues were reviewed against `main` after the
Nexus app catalog merge:

| Issue | Finding | Resolution |
| --- | --- | --- |
| #3 | Historical security advisory snapshot | Remediated by the dependency security pass and the database bincode removal. |
| #4 | Older freshness snapshot | Superseded by the later reports. |
| #5 | Older freshness snapshot | Superseded by the later reports. |
| #7 | Older freshness snapshot | Superseded by the later report #8. |
| #8 | Latest freshness snapshot | Security is clear; remaining major upgrades are intentionally deferred because they are breaking toolchain migrations. |

## Verification

The Stack Control scanner was run against the top-level workspace surfaces with
checked-out submodules excluded from this report:

- 121 `package.json` files, 21 Cargo manifests, and one Python requirements file
- 0 OSV security advisories
- 0 unsupported semantic API findings
- 2 major-version lags, both in the Nexus Project development toolchain:
  `@biomejs/biome` 1.x → 2.x and TypeScript 5.x → 7.x (with Bun types 1.3 →
  1.4 coupled to the same toolchain decision)

The current non-breaking policy keeps those migrations out of the release
branch. Same-major freshness updates can be handled independently after their
compatibility gates are defined. Separate submodules retain their own dependency
ownership and are not silently changed by the hosted platform merge.
