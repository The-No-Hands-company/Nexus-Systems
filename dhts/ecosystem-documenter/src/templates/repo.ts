import type { RepoConfig } from "../types";

function boolLabel(value: boolean): string {
  return value ? "Yes" : "No";
}

export function repoOverviewTemplate(repo: RepoConfig): string {
  return `# ${repo.name} Overview\n\n## Identity\n\n- ID: ${repo.id}\n- Category: ${repo.category}\n- Source Path: ${repo.repoPath}\n\n## Delivery Model\n\n- Deployment: ${repo.deploymentModel.join(", ")}\n- Federated: ${boolLabel(repo.federation)}\n- Embedded In Nexus-Cloud: ${boolLabel(repo.embeddedInNexusCloud)}\n- Public Facing: ${boolLabel(repo.publicFacing)}\n\n## Purpose\n\nDescribe the core outcome this tool is responsible for in the ecosystem.\n\n## Key Capabilities\n\n- Capability 1\n- Capability 2\n- Capability 3\n\n## Dependencies\n\n- Upstream systems:\n- Internal dependencies:\n- External dependencies:\n`;
}

export function architectureTemplate(repo: RepoConfig): string {
  return `# ${repo.name} Architecture\n\n## System Context\n\nHow ${repo.name} fits into Nexus Systems and where it exchanges data.\n\n## Components\n\n- API layer\n- Core domain logic\n- Data layer\n- Background workers\n\n## Interfaces\n\n- Inbound interfaces:\n- Outbound interfaces:\n\n## Data Model\n\n- Primary entities:\n- Persistence technology:\n- Retention policy:\n\n## Diagram\n\nAdd architecture diagrams here.\n`;
}

export function developerGuideTemplate(repo: RepoConfig): string {
  return `# ${repo.name} Developer Guide\n\n## Local Setup\n\n- Prerequisites:\n- Installation steps:\n- Environment variables:\n\n## Build and Run\n\n- Build command:\n- Dev command:\n- Test command:\n\n## Coding Standards\n\n- Linting:\n- Formatting:\n- Branch strategy:\n\n## Common Tasks\n\n- Add a feature\n- Add tests\n- Release a change\n`;
}

export function userGuideTemplate(repo: RepoConfig): string {
  return `# ${repo.name} User Guide\n\n## Who This Is For\n\nDescribe primary users and teams.\n\n## Core Workflows\n\n1. Workflow one\n2. Workflow two\n3. Workflow three\n\n## Configuration\n\n- Required settings:\n- Optional settings:\n\n## Troubleshooting\n\n- Symptom:\n- Likely cause:\n- Resolution:\n\n## FAQ\n\n- Question:\n  - Answer:\n`;
}

export function operationsTemplate(repo: RepoConfig): string {
  return `# ${repo.name} Operations Runbook\n\n## Service Ownership\n\n- Team:\n- On-call group:\n- Escalation path:\n\n## Deployment\n\n- Environments:\n- Deployment process:\n- Rollback process:\n\n## Monitoring\n\n- Golden signals:\n- Dashboards:\n- Alert thresholds:\n\n## Incident Response\n\n- First response checklist\n- Recovery checklist\n- Postmortem checklist\n`;
}

export function apiTemplate(repo: RepoConfig): string {
  return `# ${repo.name} API and Contracts\n\n## API Surface\n\nDocument endpoint, message, or interface contracts used by ${repo.name}.\n\n## Authentication\n\n- Auth mechanism:\n- Token or key scopes:\n\n## Versioning\n\n- Current version:\n- Breaking change policy:\n\n## Endpoints or Interfaces\n\n| Name | Method/Type | Path/Topic | Purpose |
| --- | --- | --- | --- |
| Example | GET | /health | Health probe |
\n## Errors\n\n| Code | Meaning | Caller Action |
| --- | --- | --- |
| 400 | Invalid input | Fix request |
`;
}
