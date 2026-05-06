# Security Policy

## Supported Scope

Security issues related to Nexus Engine source code, build pipeline, dependencies, and scripting/runtime sandbox boundaries are in scope.

## Reporting a Vulnerability

Please report vulnerabilities privately to project maintainers. Do not open public issues with exploit details before triage.

Include:

- Affected component and version/commit
- Reproduction steps
- Impact assessment
- Suggested remediation, if available

## Response Process

- Acknowledge report receipt.
- Triage severity and impact.
- Prepare fix and validation.
- Coordinate responsible disclosure timeline.

## Hardening Priorities

- Dependency and supply-chain hygiene
- Script sandbox boundaries
- Native-module interface validation
- Resource exhaustion and denial-of-service vectors
- Unsafe deserialization or untrusted asset ingestion paths
