export type FeatureState = "planned" | "stubbed" | "in-progress" | "ready";

export interface FeatureDefinition {
  id: string;
  name: string;
  category: string;
  state: FeatureState;
  description: string;
}

export const featureCatalog: FeatureDefinition[] = [
  { id: "repo.create", name: "Repository Creation", category: "Core Repository", state: "stubbed", description: "Create repositories with VCS selection." },
  { id: "repo.transfer", name: "Repository Transfer", category: "Core Repository", state: "planned", description: "Transfer repositories between owners and teams." },
  { id: "repo.archive", name: "Repository Archival", category: "Core Repository", state: "planned", description: "Archive repositories in read-only mode." },
  { id: "repo.mirror", name: "Repository Mirroring", category: "Core Repository", state: "planned", description: "Mirror repositories to external providers." },
  { id: "repo.templates", name: "Repository Templates", category: "Core Repository", state: "planned", description: "Create new repositories from templates." },

  { id: "branches.protection", name: "Branch Protection Rules", category: "Branching", state: "planned", description: "Restrict force pushes and direct commits." },
  { id: "branches.default", name: "Default Branch Management", category: "Branching", state: "stubbed", description: "Set and manage default branch names." },
  { id: "branches.cleanup", name: "Merged Branch Cleanup", category: "Branching", state: "planned", description: "Automatically delete merged branches." },
  { id: "branches.rulesets", name: "Rule Sets", category: "Branching", state: "planned", description: "Apply branch policies by pattern." },

  { id: "pr.create", name: "Pull Request Creation", category: "Code Review", state: "stubbed", description: "Create and track pull requests." },
  { id: "pr.review", name: "Review Workflows", category: "Code Review", state: "stubbed", description: "Approve/request changes workflows." },
  { id: "pr.merge-queue", name: "Merge Queue", category: "Code Review", state: "planned", description: "Queue merges with linear history guarantees." },
  { id: "pr.auto-merge", name: "Auto Merge", category: "Code Review", state: "planned", description: "Merge once checks and reviews pass." },
  { id: "pr.stacked", name: "Stacked Pull Requests", category: "Code Review", state: "planned", description: "Support dependent PR chains." },

  { id: "issues.create", name: "Issue Tracking", category: "Project Management", state: "stubbed", description: "Create and manage issues." },
  { id: "issues.templates", name: "Issue Templates", category: "Project Management", state: "planned", description: "Reusable issue forms and templates." },
  { id: "issues.automation", name: "Issue Automation", category: "Project Management", state: "planned", description: "Automate labels, assignment, and triage." },
  { id: "projects.kanban", name: "Kanban Boards", category: "Project Management", state: "planned", description: "Track issue and PR progress on boards." },
  { id: "milestones", name: "Milestones", category: "Project Management", state: "planned", description: "Group work by release goals." },

  { id: "actions.pipelines", name: "CI Pipelines", category: "CI/CD", state: "stubbed", description: "Configure jobs on push and pull requests." },
  { id: "actions.runners", name: "Self-hosted Runners", category: "CI/CD", state: "planned", description: "Scale jobs across custom runners." },
  { id: "actions.secrets", name: "Secrets Management", category: "CI/CD", state: "planned", description: "Encrypted secrets for workflows." },
  { id: "actions.cache", name: "Build Cache", category: "CI/CD", state: "planned", description: "Cache dependencies and artifacts." },
  { id: "deploy.environments", name: "Deployment Environments", category: "CI/CD", state: "planned", description: "Promote releases across environments." },

  { id: "packages.npm", name: "NPM Registry", category: "Packages", state: "planned", description: "Publish and consume JS packages." },
  { id: "packages.container", name: "Container Registry", category: "Packages", state: "planned", description: "Host OCI and Docker images." },
  { id: "packages.maven", name: "Maven Registry", category: "Packages", state: "planned", description: "Store Java artifacts." },
  { id: "packages.python", name: "PyPI Registry", category: "Packages", state: "planned", description: "Host Python packages." },

  { id: "security.sast", name: "Static Analysis", category: "Security", state: "planned", description: "Scan code for security issues." },
  { id: "security.secret-scan", name: "Secret Scanning", category: "Security", state: "planned", description: "Detect leaked credentials." },
  { id: "security.dep-scan", name: "Dependency Scanning", category: "Security", state: "planned", description: "Find vulnerable dependencies." },
  { id: "security.codeowners", name: "Code Owners", category: "Security", state: "planned", description: "Require owner approval for sensitive paths." },
  { id: "security.signing", name: "Commit Signing Policies", category: "Security", state: "planned", description: "Enforce signed commits and tags." },

  { id: "wiki.pages", name: "Repository Wiki", category: "Documentation", state: "planned", description: "Versioned documentation pages." },
  { id: "docs.site", name: "Docs Hosting", category: "Documentation", state: "planned", description: "Publish docs site from repository." },
  { id: "snippets", name: "Code Snippets", category: "Documentation", state: "planned", description: "Reusable snippet library." },
  { id: "adr", name: "Architecture Decisions", category: "Documentation", state: "planned", description: "Store and track ADR records." },

  { id: "discussion.forum", name: "Discussions", category: "Community", state: "planned", description: "Community discussion boards." },
  { id: "discussion.announcements", name: "Announcements", category: "Community", state: "planned", description: "Broadcast product and repo updates." },
  { id: "discussion.qna", name: "Q&A", category: "Community", state: "planned", description: "Question and answer support flows." },

  { id: "notifications.inbox", name: "Unified Inbox", category: "Notifications", state: "planned", description: "Centralized notifications stream." },
  { id: "notifications.email", name: "Email Notifications", category: "Notifications", state: "planned", description: "Send activity updates by email." },
  { id: "notifications.webhook", name: "Webhook Notifications", category: "Notifications", state: "stubbed", description: "Push event payloads to external services." },
  { id: "notifications.push", name: "Push Notifications", category: "Notifications", state: "planned", description: "Browser and mobile push alerts." },

  { id: "integrations.slack", name: "Slack Integration", category: "Integrations", state: "planned", description: "Post events to Slack channels." },
  { id: "integrations.discord", name: "Discord Integration", category: "Integrations", state: "planned", description: "Sync notifications to Discord." },
  { id: "integrations.jira", name: "Jira Sync", category: "Integrations", state: "planned", description: "Sync issues and workflow states." },
  { id: "integrations.sso", name: "SSO/SAML", category: "Integrations", state: "planned", description: "Enterprise single sign-on support." },
  { id: "integrations.ldap", name: "LDAP Auth", category: "Integrations", state: "planned", description: "Authenticate users via LDAP." },

  { id: "search.code", name: "Code Search", category: "Search", state: "planned", description: "Search across files and symbols." },
  { id: "search.semantic", name: "Semantic Search", category: "Search", state: "planned", description: "Natural language query support." },
  { id: "search.history", name: "Commit History Search", category: "Search", state: "planned", description: "Search commits and metadata." },
  { id: "search.issue", name: "Issue/PR Search", category: "Search", state: "planned", description: "Search conversations and tickets." },

  { id: "federation.discovery", name: "Peer Discovery", category: "Federation", state: "stubbed", description: "Discover peer Nexus Forge nodes." },
  { id: "federation.replication", name: "Repository Replication", category: "Federation", state: "planned", description: "Replicate repository data across peers." },
  { id: "federation.identity", name: "Cross-node Identity", category: "Federation", state: "planned", description: "Trust and identity exchange." },
  { id: "federation.failover", name: "Regional Failover", category: "Federation", state: "planned", description: "Automatic failover for resilience." },

  { id: "admin.audit", name: "Audit Logging", category: "Administration", state: "stubbed", description: "Track security and admin events." },
  { id: "admin.compliance", name: "Compliance Reports", category: "Administration", state: "planned", description: "Generate policy and compliance reports." },
  { id: "admin.retention", name: "Retention Policies", category: "Administration", state: "planned", description: "Control data retention and archival." },
  { id: "admin.backup", name: "Backup and Restore", category: "Administration", state: "stubbed", description: "Snapshot and restore repositories/metadata." },
  { id: "admin.observability", name: "Observability Dashboard", category: "Administration", state: "planned", description: "Metrics, tracing, and alerting overview." },

  { id: "ai.review", name: "AI Pull Request Review", category: "AI", state: "stubbed", description: "Automated review suggestions on PRs." },
  { id: "ai.issue-pr", name: "Issue to PR Generation", category: "AI", state: "stubbed", description: "Generate draft PRs from issues." },
  { id: "ai.test-gen", name: "AI Test Generation", category: "AI", state: "planned", description: "Generate tests for changed code." },
  { id: "ai.docs-gen", name: "AI Docs Suggestions", category: "AI", state: "planned", description: "Generate docs and release notes." },
  { id: "ai.security", name: "AI Security Advisor", category: "AI", state: "planned", description: "Risk assessment and remediation guidance." },

  { id: "ux.theming", name: "Custom Themes", category: "User Experience", state: "planned", description: "Personalized UI theming." },
  { id: "ux.shortcuts", name: "Keyboard Shortcuts", category: "User Experience", state: "planned", description: "Power-user navigation workflows." },
  { id: "ux.mobile", name: "Mobile Responsive UI", category: "User Experience", state: "stubbed", description: "Responsive web experience." },
  { id: "ux.accessibility", name: "Accessibility", category: "User Experience", state: "planned", description: "WCAG-compliant accessibility support." },

  { id: "governance.policy", name: "Policy-as-Code", category: "Governance", state: "planned", description: "Define repo/org rules as code." },
  { id: "governance.approvals", name: "Approval Policies", category: "Governance", state: "planned", description: "Multi-party approval requirements." },
  { id: "governance.rbac", name: "Advanced RBAC", category: "Governance", state: "stubbed", description: "Granular permission roles." },

  { id: "performance.sharding", name: "Repository Sharding", category: "Scalability", state: "planned", description: "Distribute repos across storage nodes." },
  { id: "performance.cache", name: "Read Cache Layer", category: "Scalability", state: "planned", description: "Accelerate common read operations." },
  { id: "performance.edge", name: "Edge Replicas", category: "Scalability", state: "planned", description: "Geo-distributed mirrors for low latency." },

  { id: "enterprise.support", name: "Enterprise Support Tools", category: "Enterprise", state: "planned", description: "Tenant diagnostics and support bundles." },
  { id: "enterprise.multi-tenant", name: "Multi-tenant Hosting", category: "Enterprise", state: "planned", description: "Host multiple organizations securely." },
  { id: "enterprise.legal", name: "Legal Hold", category: "Enterprise", state: "planned", description: "Preserve records for legal workflows." }
];

export function getFeatureSummary() {
  const summary = { total: featureCatalog.length, planned: 0, stubbed: 0, inProgress: 0, ready: 0 };
  for (const feature of featureCatalog) {
    if (feature.state === "planned") summary.planned += 1;
    if (feature.state === "stubbed") summary.stubbed += 1;
    if (feature.state === "in-progress") summary.inProgress += 1;
    if (feature.state === "ready") summary.ready += 1;
  }
  return summary;
}
