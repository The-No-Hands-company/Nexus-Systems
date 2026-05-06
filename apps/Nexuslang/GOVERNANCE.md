# NexusLang Governance

## Overview

NLPL is an open-source project maintained by The No-Hands Company. This document
describes how decisions are made, who has commit access, and how the project evolves
over time.

## Roles

### Users

Anyone who downloads, runs, or reads about NexusLang. No formal requirements. All are
welcome to open issues and participate in discussions.

### Contributors

Anyone who submits a pull request that is merged, or regularly participates in issue
discussions. Contributors are listed in the repository's contributors graph.

### Committers

Contributors who have demonstrated sustained, high-quality contributions and have been
granted write access to the repository by a maintainer. Committers can:

- Merge pull requests (after review)
- Triage and label issues
- Approve CI pipelines

Committer status is proposed by any existing committer and approved by a simple
majority of current maintainers.

### Maintainers

Responsible for the overall direction of the project. Currently:

- The No-Hands Company (founding maintainer)

Maintainers can:

- Grant or revoke committer status
- Make binding decisions on RFCs
- Publish releases
- Manage project infrastructure

New maintainers are appointed by unanimous agreement of existing maintainers.

## Decision Making

### Everyday decisions

Bug fixes, documentation improvements, small features, and test additions that align
clearly with the project roadmap are decided by any committer with a single approval
from another committer or maintainer.

### Significant changes

Changes that affect the language syntax, type system, runtime semantics, or public
APIs require:

1. An RFC issue opened using the RFC template
2. A public comment period of at least 7 days
3. Approval from at least one maintainer

### Breaking changes

Changes that break backward compatibility also require:

1. A migration path documented in the RFC
2. A deprecation notice in the changelog
3. Unanimous approval from all active maintainers

## Releases

NLPL uses [Semantic Versioning](https://semver.org):

- **Patch** (`0.1.x`): bug fixes, no API or syntax changes
- **Minor** (`0.x.0`): new features, backward-compatible
- **Major** (`x.0.0`): breaking changes to language syntax, type system, or runtime API

Release cadence is determined by the maintainers based on readiness, not a fixed schedule.

## RFC Process

1. Open an issue using the RFC template
2. Maintainers assign an RFC number and set status to `Active`
3. Discussion period (minimum 7 days for minor RFCs, 14 days for major)
4. Maintainer calls the decision: `Accepted`, `Rejected`, or `Deferred`
5. Accepted RFCs are implemented via one or more pull requests
6. The RFC issue is closed when the last implementation PR is merged

## Code of Conduct

All participants are expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
Violations can be reported by emailing the maintainers directly. Reports are
handled confidentially.

## Amendments

This governance document can be amended by any maintainer with consent from all other
active maintainers. Proposed amendments should be submitted as a pull request against
this file with at least 7 days for community comment before merging.
