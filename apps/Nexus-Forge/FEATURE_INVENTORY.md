# Nexus Forge — Feature Inventory

A living document tracking every feature, its current implementation status, and what still needs to be stubbed or built. Modeled on a three-tier development ladder.

## Status Legend

| Symbol | Status | Meaning |
|--------|--------|---------|
| ✅ | **IMPLEMENTED** | Real working code, persisted to storage, tested |
| 🟡 | **STUB** | Route exists, returns mock data, frontend renders |
| ❌ | **NOT YET** | Not yet stubbed or implemented |

---

## 1. Core Version Control System (VCS)

### 1.1 Repository Management
| Feature | Status | Notes |
|---------|--------|-------|
| List repositories | ✅ | `GET /api/repos` — real data from SQLite |
| Create repository | ✅ | `POST /api/repos` — initializes bare git repo |
| Get repository detail | ✅ | `GET /api/repos/:name` — real data |
| Repository activity feed | ✅ | `GET /api/repos/:name/activity` |
| Repository settings (description, visibility) | 🟡 | vcs-advanced.ts |
| Repository archiving | ❌ | |
| Repository forking | ❌ | |
| Repository templates | ❌ | |
| Repository transfer (between orgs/users) | ❌ | |
| Repository import from URL | ❌ | |
| Repository export / bundle download | ❌ | |
| Repository statistics (commit freq, contributors) | ❌ | |
| Storage quota tracking | ❌ | |
| Garbage collection / repo maintenance | ❌ | |
| Repository rename | ❌ | |
| Repository visibility (public/private/internal) | ❌ | |

### 1.2 Branches
| Feature | Status | Notes |
|---------|--------|-------|
| List branches | 🟡 | vcs-advanced.ts |
| Create / delete branch | 🟡 | vcs-advanced.ts |
| Default branch configuration | 🟡 | vcs-advanced.ts |
| Branch protection policies | 🟡 | branch-protection-policies.ts |
| Required reviews before merge | ❌ | |
| Required status checks | ❌ | |
| Branch staleness tracking | ❌ | |
| Automated branch cleanup | ❌ | |
| Branch comparison view | ❌ | |
| Branch strategy enforcement (GitFlow, trunk-based) | ❌ | |

### 1.3 Commits
| Feature | Status | Notes |
|---------|--------|-------|
| View commit history | 🟡 | vcs-advanced.ts |
| Commit detail / message | 🟡 | vcs-advanced.ts |
| Commit signing & verification | 🟡 | commit-signing-vault.ts |
| GPG / SSH key management | 🟡 | commit-signing-vault.ts |
| Commit search | ❌ | |
| Cherry-pick operation | ❌ | |
| Revert commit | ❌ | |
| Amend commit (via rebase) | ❌ | |
| Commit message linting | ❌ | |
| Conventional commit enforcement | ❌ | |
| Commit co-authors | ❌ | |
| Reflog viewer | ❌ | |

### 1.4 Merge & Rebase
| Feature | Status | Notes |
|---------|--------|-------|
| Merge strategies (fast-forward, squash, rebase) | 🟡 | merge-strategy-engine.ts |
| Conflict detection | 🟡 | merge-strategy-engine.ts |
| Conflict resolution UI | 🟡 | merge-strategy-engine.ts |
| Automatic merge queue | ❌ | |
| Merge train (sequential merge validation) | ❌ | |
| Rebase operation | ❌ | |
| Interactive rebase | ❌ | |
| Stash management | ❌ | |
| Bisect (binary search for regression) | ❌ | |
| Worktree support | ❌ | |

### 1.5 Diffs & Code Viewing
| Feature | Status | Notes |
|---------|--------|-------|
| File diff viewer | ❌ | |
| Side-by-side diff | ❌ | |
| Patch generation and application | ❌ | |
| Blame / annotate view | ❌ | |
| File history (per file) | ❌ | |
| Directory tree browser | ❌ | |
| Syntax-highlighted source view | ❌ | |
| Raw file download | ❌ | |
| Code search (within repo) | ❌ | |
| Cross-repo code search | 🟡 | semantic-search-index.ts |
| Symbol / function navigation | ❌ | |

### 1.6 Tags & Releases
| Feature | Status | Notes |
|---------|--------|-------|
| Create / list / delete tags | ❌ | |
| Lightweight vs annotated tags | ❌ | |
| Release creation from tag | ❌ | |
| Release notes auto-generation | ❌ | |
| Asset upload to releases | ❌ | |
| Release channels (stable, beta, nightly) | 🟡 | release-engineering.ts |
| Changelog generation | ❌ | |

### 1.7 Submodules & LFS
| Feature | Status | Notes |
|---------|--------|-------|
| Git submodule management | ❌ | |
| Git LFS (large file storage) | ❌ | |
| LFS quota and billing | ❌ | |
| LFS migration tooling | ❌ | |

---

## 2. Pull Requests & Code Review

### 2.1 Pull Requests
| Feature | Status | Notes |
|---------|--------|-------|
| Create / list / close pull requests | 🟡 | pull-requests page exists |
| PR templates | ❌ | |
| Draft PRs | ❌ | |
| PR labels and milestones | ❌ | |
| PR reviewers assignment | ❌ | |
| PR auto-assignment rules | ❌ | |
| PR size labeling | ❌ | |
| PR linked issues | ❌ | |
| PR status checks integration | ❌ | |
| PR merge queue | ❌ | |
| PR activity timeline | ❌ | |
| PR comparison base selection | ❌ | |
| PR stacked branches | ❌ | |
| PR revert button | ❌ | |

### 2.2 Code Review
| Feature | Status | Notes |
|---------|--------|-------|
| Inline comment threads on diffs | ❌ | |
| Review approval (approve / request changes / comment) | ❌ | |
| Review suggestions (accept to auto-commit) | ❌ | |
| Review resolution tracking | ❌ | |
| Review analytics and cycle time | ❌ | |
| Code review templates | ❌ | |
| AI-assisted code review | 🟡 | AGENTS.md describes the workflow |
| Suggested reviewers (ML-based) | ❌ | |
| Code ownership review requirements | 🟡 | code-ownership-tracker.ts |
| Review reminders / SLA enforcement | ❌ | |

---

## 3. Issues & Project Tracking

### 3.1 Issues
| Feature | Status | Notes |
|---------|--------|-------|
| Create / list / close issues | ❌ | No dedicated issue tracker yet |
| Issue templates | ❌ | |
| Issue labels (create, color, assign) | ❌ | |
| Issue milestones | ❌ | |
| Issue assignment to users | ❌ | |
| Issue linking to commits / PRs | ❌ | |
| Issue mentions (@user, #issue) | ❌ | |
| Issue locking | ❌ | |
| Issue transfer between repos | ❌ | |
| Issue search and filtering | ❌ | |
| Issue subscriptions / notifications | 🟡 | notification-orchestration.ts |
| Issue SLA / deadline tracking | ❌ | |
| Issue automation rules | ❌ | |

### 3.2 Project Boards
| Feature | Status | Notes |
|---------|--------|-------|
| Kanban board | ❌ | |
| Scrum board with sprints | ❌ | |
| Backlog management | 🟡 | autonomous-backlog-triage.ts |
| Milestone / epic grouping | ❌ | |
| Burndown charts | ❌ | |
| Velocity tracking | ❌ | |
| Roadmap view (Gantt-style) | 🟡 | roadmap-dependency-simulator.ts |
| Dependency linking between issues | 🟡 | cross-product-dependency-intelligence.ts |
| OKR alignment | 🟡 | strategy-okr.ts |

---

## 4. CI/CD Pipelines

### 4.1 Pipeline Engine
| Feature | Status | Notes |
|---------|--------|-------|
| Pipeline definition (YAML-based) | ❌ | |
| Pipeline trigger rules (push, PR, schedule, manual) | ❌ | |
| Pipeline execution and logging | ❌ | |
| Pipeline visualization (DAG view) | ❌ | |
| Pipeline caching | ❌ | |
| Pipeline artifacts | ❌ | |
| Matrix builds | ❌ | |
| Conditional steps | ❌ | |
| Pipeline templates / reusable workflows | 🟡 | workflows.ts |
| Pipeline approval gates | ❌ | |
| Pipeline retry logic | ❌ | |
| Parallel job execution | ❌ | |

### 4.2 Runners & Environments
| Feature | Status | Notes |
|---------|--------|-------|
| Self-hosted runner registration | ❌ | |
| Cloud runner provisioning | ❌ | |
| Runner groups | ❌ | |
| Environment definitions (dev/staging/prod) | ❌ | |
| Environment secrets and variables | 🟡 | secrets-vault.ts |
| Deployment environments with protection rules | ❌ | |
| Container-based runners (Docker) | ❌ | |
| Kubernetes runner executor | ❌ | |

### 4.3 Release Engineering
| Feature | Status | Notes |
|---------|--------|-------|
| Release pipelines | 🟡 | release-engineering.ts |
| Release reliability scorecards | 🟡 | release-reliability-scorecards.ts |
| Release cadence optimizer | 🟡 | release-cadence-optimizer.ts |
| Autonomous release governor | 🟡 | autonomous-release-governor.ts |
| Canary / blue-green deployment integration | ❌ | |
| Feature flag integration with releases | 🟡 | feature-gating-orchestrator.ts |
| Rollback automation | ❌ | |
| Post-deployment validation | ❌ | |

---

## 5. Identity, Access & Security

### 5.1 Authentication
| Feature | Status | Notes |
|---------|--------|-------|
| User registration / login | 🟡 | auth.ts exists |
| Password hashing and salting | ❌ | auth.ts is stub |
| OAuth2 / OpenID Connect (GitHub, GitLab, Google) | ❌ | |
| SAML SSO | ❌ | |
| SCIM provisioning | ❌ | |
| Multi-factor authentication (TOTP, WebAuthn) | ❌ | |
| Personal access tokens | ❌ | |
| Deploy keys (repo-scoped SSH) | ❌ | |
| SSH key management | ❌ | |
| Session management | ❌ | |

### 5.2 Authorization
| Feature | Status | Notes |
|---------|--------|-------|
| Role-based access control | 🟡 | permissions.ts |
| Permission levels (read / write / admin) | 🟡 | permissions.ts |
| Organization-level roles | ❌ | |
| Team-level roles | 🟡 | teams page + collaboration.ts |
| Repo-level access overrides | ❌ | |
| IP allowlist / denylist | ❌ | |
| Audit log (who did what, when) | 🟡 | audit-intelligence.ts |
| CODEOWNERS enforcement | 🟡 | code-ownership-tracker.ts |

### 5.3 Secrets & Credentials
| Feature | Status | Notes |
|---------|--------|-------|
| Secrets vault | 🟡 | secrets-vault.ts |
| Developer credential graph | 🟡 | developer-credential-graph.ts |
| Secret scanning (push protection) | ❌ | |
| Secret rotation automation | ❌ | |
| Environment variable management | ❌ | |
| PKI / certificate management | ❌ | |

### 5.4 Compliance & Governance
| Feature | Status | Notes |
|---------|--------|-------|
| Governance risk council | 🟡 | governance-risk-council.ts |
| Policy as code studio | 🟡 | policy-as-code-studio.ts |
| Policy conflict resolver | 🟡 | policy-conflict-resolver.ts |
| Adaptive compliance orchestrator | 🟡 | adaptive-compliance-orchestrator.ts |
| Ecosystem compliance exchange | 🟡 | ecosystem-compliance-exchange.ts |
| Legislative change radar | 🟡 | legislative-change-radar.ts |
| Customer data privacy vault (GDPR/CCPA) | 🟡 | customer-data-privacy-vault.ts |
| License compliance tracker | 🟡 | license-compliance-tracker.ts |
| Digital ethics governance | 🟡 | digital-ethics-governance.ts |
| Data governance fabric | 🟡 | data-governance-fabric.ts |

---

## 6. Multi-VCS Federation

| Feature | Status | Notes |
|---------|--------|-------|
| VCS abstraction layer (interface) | ✅ | vcs-interface.ts |
| Git backend | ✅ | git-backend.ts |
| Cross-VCS repository sync | 🟡 | cross-vcs-repository-sync.ts |
| Federation endpoint (Nexus Cloud connect) | 🟡 | federation.ts |
| GitHub mirror / import | ❌ | |
| GitLab mirror / import | ❌ | |
| Bitbucket mirror / import | ❌ | |
| Azure DevOps bridge | ❌ | |
| SVN migration tooling | ❌ | |
| Mercurial bridge | ❌ | |
| Perforce bridge | ❌ | |
| Unified commit history across backends | ❌ | |
| Cross-VCS branch mapping | ❌ | |
| VCS metadata normalization layer | ❌ | |
| Federation health dashboard | 🟡 | cross-vcs-repository-sync.ts |

---

## 7. Webhooks, APIs & Integrations

### 7.1 Webhooks
| Feature | Status | Notes |
|---------|--------|-------|
| Webhook creation (repo / org level) | ❌ | |
| Webhook event selection | ❌ | |
| Webhook delivery logs and retry | ❌ | |
| Webhook signature verification | ❌ | |
| Integration exchange hub | 🟡 | integration-exchange.ts |
| Interoperability hub | 🟡 | interop.ts |

### 7.2 API Gateway
| Feature | Status | Notes |
|---------|--------|-------|
| REST API (OpenAPI spec) | ❌ | |
| GraphQL API | ❌ | |
| API versioning | ❌ | |
| API rate limiting | ❌ | |
| API management hub | 🟡 | api-management.ts |
| API evolution control tower | 🟡 | api-evolution-control-tower.ts |
| OAuth2 app registration | ❌ | |

### 7.3 Package Registries
| Feature | Status | Notes |
|---------|--------|-------|
| npm registry | ❌ | |
| Docker / OCI container registry | ❌ | |
| Maven / Gradle registry | ❌ | |
| PyPI registry | ❌ | |
| Helm chart registry | ❌ | |
| NuGet registry | ❌ | |
| Generic artifact registry | 🟡 | artifact-trust-chain.ts |
| Package trust / signing chain | 🟡 | artifact-trust-chain.ts |

---

## 8. Developer Experience

### 8.1 Web IDE & Code Editing
| Feature | Status | Notes |
|---------|--------|-------|
| Browser-based code editor | ❌ | |
| Syntax highlighting | ❌ | |
| Find / replace across files | ❌ | |
| File creation / rename / delete in browser | ❌ | |
| Commit directly from web editor | ❌ | |
| Quick edit (single file, no full IDE) | ❌ | |
| Dev container / Codespace-like environments | ❌ | |

### 8.2 Developer Tools
| Feature | Status | Notes |
|---------|--------|-------|
| Developer experience hub | 🟡 | developer-experience.ts |
| Developer journey intelligence | 🟡 | developer-journey-intelligence.ts |
| Engineering cognition workspace | 🟡 | engineering-cognition-workspace.ts |
| Semantic search index | 🟡 | semantic-search-index.ts |
| CLI tool (forge CLI) | ❌ | |
| Desktop client | ❌ | |
| IDE extensions (VS Code, JetBrains) | ❌ | |
| Git credential helper | ❌ | |
| Developer portals / internal portals | ❌ | |

### 8.3 Notebooks & Experimentation
| Feature | Status | Notes |
|---------|--------|-------|
| Notebook hub | 🟡 | notebooks.ts |
| Experimentation platform | 🟡 | experimentation-platform.ts |
| Research lab ops | 🟡 | research-lab-ops.ts |
| Product experiment ledger | 🟡 | product-experiment-ledger.ts |

---

## 9. AI & Machine Learning

### 9.1 AI Features
| Feature | Status | Notes |
|---------|--------|-------|
| AI routes hub | 🟡 | ai.ts |
| AI safety ops | 🟡 | ai-safety-ops.ts |
| AI capability inventory | 🟡 | ai-capability-inventory.ts |
| Model marketplace ops | 🟡 | model-marketplace-ops.ts |
| MLOps hub | 🟡 | mlops.ts |
| Digital twin | 🟡 | digital-twin.ts |
| Quantum resilience lab | 🟡 | quantum-resilience-lab.ts |
| AI-assisted code review | ❌ | Described in AGENTS.md, not yet wired |
| Auto-generate PR from issue | ❌ | Described in AGENTS.md, not yet wired |
| Agent task queue | ❌ | Described in AGENTS.md, not yet wired |

---

## 10. Observability & Operations

### 10.1 Monitoring
| Feature | Status | Notes |
|---------|--------|-------|
| Observability hub | 🟡 | observability.ts |
| Incident management | 🟡 | incident.ts |
| Incident learning system | 🟡 | incident-learning-system.ts |
| Incident prevention graph | 🟡 | incident-prevention-graph.ts |
| Operational narrative studio | 🟡 | operational-narrative-studio.ts |
| Platform telemetry | 🟡 | product-telemetry-intel.ts |
| Cloud cost anomaly lab | 🟡 | cloud-cost-anomaly-lab.ts |
| SLI / SLO tracking | 🟡 | service-level-economics.ts |
| Error budget management | ❌ | |
| On-call rotation management | ❌ | |
| Alert routing rules | ❌ | |
| Log aggregation | ❌ | |
| Distributed tracing | ❌ | |
| Metrics dashboards | ❌ | |
| Real-time status page | ❌ | |

### 10.2 Infrastructure
| Feature | Status | Notes |
|---------|--------|-------|
| Infrastructure portfolio | 🟡 | infrastructure-portfolio.ts |
| Infrastructure optimization lab | 🟡 | infrastructure-optimization-lab.ts |
| Edge hub | 🟡 | edge.ts |
| Sovereign cloud ops | 🟡 | sovereign-cloud-ops.ts |
| Multi-cloud failover mesh | 🟡 | multi-cloud-failover-mesh.ts |
| Cross-cloud orchestration | 🟡 | cross-cloud-orchestration.ts |
| Workload orchestration engine | 🟡 | workload-orchestration-engine.ts |
| Disaster recovery | 🟡 | disaster-recovery.ts |

### 10.3 Resilience & Security Ops
| Feature | Status | Notes |
|---------|--------|-------|
| Resilience hub | 🟡 | resilience.ts |
| Security chaos engineering | 🟡 | security-chaos-engineering.ts |
| Platform trust automation | 🟡 | platform-trust-automation.ts |
| Trust center | 🟡 | trust-center.ts |
| Trust analytics | 🟡 | trust-analytics.ts |
| Runtime governance | 🟡 | runtime-governance.ts |

---

## 11. Organization & Teams

| Feature | Status | Notes |
|---------|--------|-------|
| Team management | 🟡 | teams page + collaboration.ts |
| Org design studio | 🟡 | org-design-studio.ts |
| Hybrid workforce router | 🟡 | hybrid-workforce-router.ts |
| Workforce skill marketplace | 🟡 | workforce-skill-marketplace.ts |
| Organization settings | ❌ | |
| Namespace / group hierarchy | ❌ | |
| Organization-level webhooks | ❌ | |
| Member directory | ❌ | |
| Onboarding flows | ❌ | |
| Department / cost center mapping | ❌ | |

---

## 12. Finance, Billing & FinOps

| Feature | Status | Notes |
|---------|--------|-------|
| FinOps hub | 🟡 | finops.ts |
| Billing core | 🟡 | billing-core.ts |
| Platform economics | 🟡 | platform-economics.ts |
| Budget allocation engine | 🟡 | budget-allocation-engine.ts |
| Monetization | 🟡 | monetization.ts |
| Service level economics | 🟡 | service-level-economics.ts |
| Demand forecasting grid | 🟡 | demand-forecasting-grid.ts |
| Revenue hub | 🟡 | revenue.tsx |
| Invoicing | ❌ | |
| Payment gateway integration | ❌ | |
| Usage-based billing | ❌ | |
| Subscription management | ❌ | |
| Free tier / trial management | ❌ | |

---

## 13. Knowledge, Docs & Content

| Feature | Status | Notes |
|---------|--------|-------|
| Knowledge hub | 🟡 | knowledge.ts |
| Enterprise knowledge graph | 🟡 | enterprise-knowledge-graph.ts |
| Learning center | 🟡 | learning-center.ts |
| Content studio | 🟡 | content-studio.ts |
| Wiki | ❌ | |
| README rendering (Markdown) | ❌ | |
| Auto-generated documentation | ❌ | |
| Nexus Pages (static site hosting) | ❌ | |
| Documentation versioning | ❌ | |
| API reference auto-generation | ❌ | |
| Machine translation platform | 🟡 | machine-translation-platform.ts |
| Globalization readiness | 🟡 | globalization-readiness.ts |
| Localization hub | 🟡 | localization.ts |

---

## 14. Collaboration & Communication

| Feature | Status | Notes |
|---------|--------|-------|
| Collaboration hub | 🟡 | collaboration.ts |
| ChatOps hub | 🟡 | chatops.ts |
| Communications hub | 🟡 | communications.ts |
| Notification orchestration | 🟡 | notification-orchestration.ts |
| Stakeholder communication intelligence | 🟡 | stakeholder-communication-intelligence.ts |
| @mention support | ❌ | |
| Discussion threads (GitHub-style) | ❌ | |
| Announcement channels | ❌ | |
| Real-time presence indicators | ❌ | |

---

## 15. Analytics & Intelligence

| Feature | Status | Notes |
|---------|--------|-------|
| Analytics hub | 🟡 | analytics.ts |
| Product analytics intelligence | 🟡 | product-analytics-intelligence.ts |
| Customer value observatory | 🟡 | customer-value-observatory.ts |
| Customer journey orchestration | 🟡 | customer-journey-orchestration.ts |
| Customer lifetime vault | 🟡 | customer-lifetime-vault.ts |
| Product telemetry intel | 🟡 | product-telemetry-intel.ts |
| Capability maturity radar | 🟡 | capability-maturity-radar.ts |
| Portfolio synergy engine | 🟡 | portfolio-synergy-engine.ts |
| Feature gating orchestrator | 🟡 | feature-gating-orchestrator.ts |
| Developer credential graph | 🟡 | developer-credential-graph.ts |
| Cross-product dependency intelligence | 🟡 | cross-product-dependency-intelligence.ts |
| Cross-team dependency graph | 🟡 | cross-team-dependency-graph.ts |
| Engineering cognition workspace | 🟡 | engineering-cognition-workspace.ts |
| Technical debt keeper | 🟡 | technical-debt-keeper.ts |

---

## 16. Data & Storage

| Feature | Status | Notes |
|---------|--------|-------|
| Data lifecycle | 🟡 | data-lifecycle.ts |
| Data governance fabric | 🟡 | data-governance-fabric.ts |
| Data contract registry | 🟡 | data-contract-registry.ts |
| Multi-tenant data isolation | 🟡 | multi-tenant-data-isolation.ts |
| Tenancy hub | 🟡 | tenancy.ts |
| Semantic search index | 🟡 | semantic-search-index.ts |
| Database schema management | ❌ | |
| Backup and restore | ❌ | |
| Data archival | ❌ | |
| Data export (GDPR portability) | ❌ | |
| Real-time event streaming | 🟡 | event-stream.ts |

---

## 17. Marketplace & Plugins

| Feature | Status | Notes |
|---------|--------|-------|
| Marketplace hub | 🟡 | marketplace.ts |
| Workflow marketplace | 🟡 | workflow-marketplace.ts |
| Plugin sandbox | 🟡 | plugin-sandbox.ts |
| Model marketplace ops | 🟡 | model-marketplace-ops.ts |
| Extension / app installation | ❌ | |
| Extension permissions model | ❌ | |
| Extension review and approval | ❌ | |
| Revenue sharing for extension authors | ❌ | |

---

## 18. Admin & Platform Config

| Feature | Status | Notes |
|---------|--------|-------|
| Admin console hub | 🟡 | admin-console.ts |
| Service catalog | 🟡 | service-catalog.ts |
| Platform hub | 🟡 | platform.ts |
| Domain workbench | 🟡 | domains.ts |
| Agent routing | 🟡 | agents.ts |
| Future labs | 🟡 | future-labs.ts |
| Hooks infrastructure | 🟡 | hooks.ts |
| Workspace management | 🟡 | workspace.ts |
| Identity hub | 🟡 | identity.ts |
| Platform goal cascade | 🟡 | platform-goal-cascade.ts |
| Change management office | 🟡 | change-management-office.ts |
| Executive command center | 🟡 | executive-command-center.ts |
| Site reliability / uptime | ❌ | |
| Admin impersonation (with audit) | ❌ | |
| Feature flags for platform itself | ❌ | |
| Rate limiting configuration | ❌ | |
| Email / SMTP configuration | ❌ | |
| Object storage configuration (S3/MinIO) | ❌ | |

---

## 19. Supply Chain & Vendor

| Feature | Status | Notes |
|---------|--------|-------|
| Supply chain hub | 🟡 | supply-chain.ts |
| Supply chain visibility | 🟡 | supply-chain-visibility.ts |
| Supply continuity planner | 🟡 | supply-continuity-planner.ts |
| Vendor risk orchestration | 🟡 | vendor-risk-orchestration.ts |
| Partner ecosystem | 🟡 | partner-ecosystem.ts |
| Procurement hub | 🟡 | procurement.ts |
| Contract testing exchange | 🟡 | contract-testing-exchange.ts |

---

## 20. Advanced & Emerging Domains

| Feature | Status | Notes |
|---------|--------|-------|
| Sustainability command center | 🟡 | sustainability-command-center.ts |
| Carbon-aware runtime | 🟡 | carbon-aware-runtime.ts |
| Quantum resilience lab | 🟡 | quantum-resilience-lab.ts |
| Digital ethics governance | 🟡 | digital-ethics-governance.ts |
| Autonomous backlog triage | 🟡 | autonomous-backlog-triage.ts |
| Autonomous release governor | 🟡 | autonomous-release-governor.ts |
| Enterprise migration factory | 🟡 | enterprise-migration-factory.ts |
| Architecture decision hub | 🟡 | architecture-decision-hub.ts |
| Policy simulation | 🟡 | policy-simulation.ts |
| Dependency remediation ops | 🟡 | dependency-remediation-ops.ts |
| Legal ops | 🟡 | legal-ops.ts |
| Quality hub | 🟡 | quality.ts |
| Mobile hub | 🟡 | mobile.ts |
| Brand consistency guard | 🟡 | brand-consistency-guard.ts |
| Performance testing lab | 🟡 | performance-testing-lab.ts |

---

## Summary Counts

| Status | Count |
|--------|-------|
| ✅ IMPLEMENTED | ~8 |
| 🟡 STUB | ~130 |
| ❌ NOT YET | ~90 |
| **Total Features Tracked** | **~228** |

---

## Priority Gaps — Next Stubs To Add

These are the highest-impact **missing stubs** that a forge platform should have before calling the surface comprehensive.

### Tier 1 — Core Forge Operations (Critical)
1. **Diff Viewer & Blame** — Every forge needs to show code, not just list repos
2. **Issue Tracker** — Issues are table-stakes for any forge platform
3. **Webhook Management** — How external tools integrate
4. **Package Registries** — npm, Docker, Helm, Maven
5. **Canary / Blue-Green Deployment** — CI/CD completeness
6. **On-call Rotation Management** — Ops completeness
7. **Real-time Status Page** — User-facing health visibility
8. **GitHub / GitLab Mirror Import** — Multi-VCS federation entry points
9. **Rollback Automation** — Release safety
10. **Web IDE / Quick Edit** — In-browser code changes

### Tier 2 — Collaboration Completeness
11. **Inline PR Review Comments** — Core review UX
12. **Discussion Threads** — GitHub-style conversations
13. **Wiki** — Documentation alongside code
14. **README Rendering** — Markdown display
15. **@mention system** — Notification routing
16. **PR merge queue** — Modern review automation
17. **Issue labels and milestones** — Project management basics
18. **Kanban/Scrum boards** — Paired with issue tracker

### Tier 3 — Enterprise Auth & Identity
19. **OAuth2 / OIDC (GitHub, GitLab, Google)** — User login
20. **SAML SSO** — Enterprise identity
21. **MFA (TOTP / WebAuthn)** — Security baseline
22. **SCIM provisioning** — Org sync
23. **Personal access tokens** — API access
24. **SSH key management** — VCS access

### Tier 4 — Registry & Packages
25. **npm Package Registry** — For JS/TS shops
26. **Docker Registry** — Container delivery
27. **Helm Chart Registry** — Kubernetes deployment
28. **Maven Registry** — Java ecosystem
29. **PyPI Registry** — Python ecosystem

---

## ⚠️ Production Readiness — Beyond Features

> Implementing every feature above produces a **feature-complete application**, not necessarily a **production-ready one**. The following layers must also be complete before Nexus Forge can be deployed at real scale. These are not features in the user-visible sense — they are the foundation everything else runs on.

---

## 21. Git Protocol Layer (The Actual VCS Engine)

> This is what fundamentally distinguishes Nexus Forge from a CRUD app. Users must be able to `git clone`, `git push`, and `git pull` over real protocols.

| Capability | Status | Notes |
|------------|--------|-------|
| HTTP Smart Git protocol (`/repo.git/info/refs`, `/repo.git/git-upload-pack`, `/repo.git/git-receive-pack`) | ✅ | git-backend.ts + main.ts handles this via `git http-backend` |
| SSH Git server (accept push/pull over SSH) | ❌ | Requires a standalone SSH daemon or ssh2 library |
| SSH host key management | ❌ | |
| Per-user SSH key authorization | ❌ | |
| Git LFS batch API endpoint | ❌ | |
| Git protocol v2 support | ❌ | |
| Pack-file bandwidth throttling | ❌ | |
| Push size limits | ❌ | |
| Pre-receive hooks execution engine | ❌ | |
| Post-receive hooks execution engine | ❌ | |
| Update hooks execution engine | ❌ | |

---

## 22. Database Layer

| Capability | Status | Notes |
|------------|--------|-------|
| SQLite database (dev) | ✅ | db.ts — used for repos, teams, activity |
| Database schema migrations (versioned) | ❌ | No migration runner (Flyway/Drizzle/custom) |
| Connection pooling | ❌ | SQLite is single-writer; needs strategy for concurrency |
| PostgreSQL support (production) | ❌ | SQLite doesn't scale horizontally |
| Read replicas | ❌ | |
| Database backup automation | ❌ | |
| Point-in-time restore | ❌ | |
| Schema validation on startup | ❌ | |
| Seed data / fixture management | ❌ | |
| Soft delete / audit trail on all entities | ❌ | |
| Full-text search indexes | ❌ | |

---

## 23. Storage Layer

| Capability | Status | Notes |
|------------|--------|-------|
| Bare git repositories on disk | ✅ | Stored under configured repo root |
| Object storage for blobs (S3 / MinIO / GCS) | ❌ | Needed for LFS, artifacts, package registries |
| CDN / edge caching for static assets | ❌ | |
| Storage quota enforcement per user/org | ❌ | |
| Orphan file cleanup (gc) | ❌ | |
| Encrypted at-rest storage | ❌ | |
| Cross-region replication | ❌ | |

---

## 24. Background Jobs & Queuing

| Capability | Status | Notes |
|------------|--------|-------|
| Agent task queue (described in AGENTS.md) | ❌ | SQLite schema defined but no worker runtime |
| Job runner / worker process | ❌ | |
| Scheduled jobs (cron-style) | ❌ | |
| Dead-letter queue | ❌ | |
| Job retry with backoff | ❌ | |
| Job observability (duration, failures) | ❌ | |
| Email delivery queue | ❌ | |
| Webhook delivery queue with retry | ❌ | |
| Priority queues (urgent vs background) | ❌ | |

---

## 25. Caching Layer

| Capability | Status | Notes |
|------------|--------|-------|
| In-memory response caching | ❌ | |
| Redis / Valkey cache backend | ❌ | |
| Cache invalidation strategy | ❌ | |
| Session store (Redis-backed) | ❌ | |
| Rate limit counters (Redis) | ❌ | |
| Distributed lock management | ❌ | |

---

## 26. Email & Notifications Infrastructure

| Capability | Status | Notes |
|------------|--------|-------|
| SMTP integration | ❌ | |
| Transactional email templates | ❌ | |
| Email verification on signup | ❌ | |
| Password reset emails | ❌ | |
| PR / issue notification emails | ❌ | |
| Digest emails (configurable frequency) | ❌ | |
| Email unsubscribe management | ❌ | |
| Slack / Teams webhook delivery | ❌ | |

---

## 27. Security Hardening

| Capability | Status | Notes |
|------------|--------|-------|
| HTTPS / TLS termination | ❌ | Currently HTTP only |
| HSTS headers | ❌ | |
| CORS policy enforcement | ❌ | |
| CSRF protection | ❌ | |
| Content Security Policy (CSP) headers | ❌ | |
| Rate limiting (per IP, per user, per token) | ❌ | |
| Request size limits | ❌ | |
| Input sanitization / XSS prevention | ❌ | |
| SQL injection prevention (parameterized queries) | ✅ | better-sqlite3 uses parameterized bindings |
| Dependency vulnerability scanning (Trivy / Snyk) | ❌ | |
| OWASP Top 10 audit | ❌ | |
| Penetration testing | ❌ | |
| Security response process (CVE disclosure) | ❌ | |

---

## 28. Observability Infrastructure

> Not the same as the observability *feature* stubs — this is the platform's own internal telemetry.

| Capability | Status | Notes |
|------------|--------|-------|
| Structured logging (JSON, log levels) | ❌ | console.log only currently |
| Log shipping (to Loki / ELK / Datadog) | ❌ | |
| Application metrics (Prometheus / OpenMetrics) | ❌ | |
| Grafana dashboards | ❌ | |
| Distributed tracing (OpenTelemetry) | ❌ | |
| Error tracking (Sentry / Bugsnag) | ❌ | |
| Health check endpoints (`/health`, `/ready`) | ❌ | |
| Uptime monitoring | ❌ | |
| Alerting rules (PagerDuty / Opsgenie) | ❌ | |
| Performance profiling | ❌ | |

---

## 29. Deployment & Infrastructure

| Capability | Status | Notes |
|------------|--------|-------|
| Dockerfile | ✅ | Dockerfile exists |
| docker-compose (dev) | ✅ | docker-compose.yml exists |
| Docker Compose production profile | ❌ | |
| Kubernetes manifests (Deployment, Service, Ingress) | ❌ | |
| Helm chart for self-hosted deployment | ❌ | |
| Horizontal Pod Autoscaling | ❌ | |
| Graceful shutdown handling (SIGTERM) | ❌ | |
| Zero-downtime deployments | ❌ | |
| Environment variable schema validation on startup | ❌ | |
| Secrets management (Vault / K8s Secrets) | ❌ | |
| CI/CD pipeline for Nexus Forge itself | ❌ | |
| Multi-region deployment topology | ❌ | |
| Backup and restore runbook | ❌ | |

---

## 30. Testing Coverage

| Capability | Status | Notes |
|------------|--------|-------|
| Unit tests (backend) | 🟡 | tests/vcs.test.ts exists — partial |
| Unit tests (frontend) | ❌ | |
| Integration tests (API layer) | ❌ | |
| End-to-end tests (Playwright / Cypress) | ❌ | |
| Git protocol tests (push/clone/pull) | ❌ | |
| Load / performance tests | ❌ | |
| Security tests (OWASP ZAP / Semgrep) | ❌ | |
| Test coverage thresholds enforced in CI | ❌ | |
| Contract tests (for federation API) | ❌ | |
| Snapshot tests (UI) | ❌ | |
| Chaos engineering tests | ❌ | |

---

## 31. Frontend Completeness

> The React frontend currently exists as a shell with navigation. A production app requires significantly more.

| Capability | Status | Notes |
|------------|--------|-------|
| Routing and navigation | ✅ | React Router, TopNav |
| Authentication flow (login/logout/register UI) | ❌ | No auth pages |
| Error boundaries | ❌ | |
| Loading states / skeletons | ❌ | |
| Empty states | ❌ | |
| Pagination | ❌ | |
| Infinite scroll | ❌ | |
| Global search UI | ❌ | |
| Keyboard shortcuts | ❌ | |
| Dark / light mode | ❌ | |
| Responsive / mobile layout | ❌ | |
| Accessibility (WCAG 2.1 AA) | ❌ | |
| Internationalization (i18n) strings | ❌ | |
| Toast / notification system | ❌ | |
| Confirmation dialogs | ❌ | |
| Drag and drop (boards, file upload) | ❌ | |
| Rich text / Markdown editor | ❌ | |
| Syntax-highlighted code renderer | ❌ | |
| Diff viewer UI component | ❌ | |
| File tree / browser UI component | ❌ | |
| Avatar / user profile system | ❌ | |

---

## 32. API Contract & Documentation

| Capability | Status | Notes |
|------------|--------|-------|
| OpenAPI / Swagger spec | ❌ | No spec exists |
| Auto-generated API reference docs | ❌ | |
| API versioning (`/v1/`, `/v2/`) | ❌ | All routes are unversioned |
| API changelog | ❌ | |
| Client SDK generation (TypeScript, Python, Go) | ❌ | |
| Postman / Bruno collection | ❌ | |
| Rate limit headers in responses | ❌ | |
| Consistent error response schema | 🟡 | contracts.ts defines some patterns |
| Pagination envelope standard | ❌ | |
| Hypermedia links (HATEOAS) | ❌ | |

---

## 33. Legal, Privacy & Compliance Operations

| Capability | Status | Notes |
|------------|--------|-------|
| Terms of Service | ❌ | |
| Privacy Policy | ❌ | |
| Cookie consent banner | ❌ | |
| Data retention policies (configurable) | ❌ | |
| Right to erasure (GDPR Article 17) implementation | ❌ | |
| Data portability export | ❌ | |
| Data Processing Agreement (DPA) template | ❌ | |
| SOC 2 Type II controls mapping | ❌ | |
| ISO 27001 controls mapping | ❌ | |
| GDPR data map | ❌ | |
| Incident response plan | ❌ | |
| Vulnerability disclosure policy | ❌ | |

---

## 34. Developer & Operator Documentation

| Capability | Status | Notes |
|------------|--------|-------|
| README.md | ✅ | Exists |
| ARCHITECTURE.md | ✅ | Exists |
| AGENTS.md | ✅ | Exists |
| ROADMAP.md | ✅ | Exists |
| Getting started / installation guide | ❌ | |
| Self-hosted deployment guide | ❌ | |
| Configuration reference (all env vars) | ❌ | |
| Upgrade / migration guide | ❌ | |
| Admin operations runbook | ❌ | |
| Contributing guide (CONTRIBUTING.md) | ❌ | |
| Code of conduct | ❌ | |
| Security policy (SECURITY.md) | ❌ | |
| Changelog (CHANGELOG.md) | ❌ | |
| Architecture decision records (ADRs) | ❌ | |

---

## ⚡ Platform Parity — What GitHub / GitLab / Gitea Have That Isn't Tracked Yet

> GitHub alone has 700+ documented API endpoints. GitLab self-reports 2,000+ individual capabilities. The sections below cover categories that exist in mature platforms but are **not yet tracked** in this document. These must be added to reach genuine parity.

---

## 35. Security Intelligence & Supply Chain (GHAS / GitLab SAST)

> GitHub Advanced Security and GitLab's security suite are entire product lines. This is not covered by the stub files.

| Feature | Status | Notes |
|---------|--------|-------|
| SAST (static application security testing) | ❌ | Semgrep / CodeQL integration |
| DAST (dynamic application security testing) | ❌ | |
| SCA (software composition analysis) | ❌ | Dependency vulnerability scanning |
| Secret scanning with push protection | ❌ | Block pushes containing secrets |
| Code scanning results in PR UI | ❌ | |
| Security advisory database integration | ❌ | CVE/GHSA lookup |
| Dependabot / auto-dependency update PRs | ❌ | |
| Dependency graph visualization | ❌ | |
| SBOM generation (CycloneDX / SPDX) | ❌ | |
| SLSA provenance attestation | ❌ | |
| Sigstore / cosign artifact signing | ❌ | |
| OIDC keyless deployment tokens (AWS/GCP/Azure) | ❌ | Replaces long-lived secrets |
| Container image scanning | ❌ | |
| Infrastructure-as-code scanning | ❌ | Terraform / Helm misconfigurations |
| Malware scanning for uploaded artifacts | ❌ | |
| License risk scoring per dependency | ❌ | |

---

## 36. Insights, Metrics & Repository Intelligence

> GitHub Insights, GitLab Value Stream Analytics, and Bitbucket Code Insights are entire analytics surfaces.

| Feature | Status | Notes |
|---------|--------|-------|
| Repository traffic (views, unique visitors, clones) | ❌ | |
| Contributor graph | ❌ | |
| Code frequency chart (additions/deletions over time) | ❌ | |
| Commit activity heatmap (punchcard view) | ❌ | |
| Network graph (fork visualization) | ❌ | |
| Dependency insights (who depends on this repo) | ❌ | |
| Value stream analytics (issue → PR → deploy cycle time) | ❌ | |
| DORA metrics dashboard (deploy frequency, MTTR, change failure rate) | ❌ | |
| Code review throughput analytics | ❌ | |
| Contributor onboarding time | ❌ | |
| PR cycle time by team | ❌ | |
| Test flakiness detection | ❌ | |
| Build time trend analysis | ❌ | |
| Repository health score | ❌ | |
| Org-level contributor leaderboard | ❌ | |
| Enterprise-level aggregated insights | ❌ | |

---

## 37. Large Repository & Monorepo Support

> At scale, standard git operations break down. GitHub, GitLab, and Bitbucket all have specific optimizations for large repos that are non-trivial to implement.

| Feature | Status | Notes |
|---------|--------|-------|
| Partial clone support | ❌ | `--filter=blob:none`, `--filter=tree:0` |
| Sparse checkout support | ❌ | |
| Shallow clone enforcement | ❌ | |
| Monorepo path-scoped CI triggers | ❌ | Only run pipelines for changed paths |
| Virtual file system (VFS for Git) | ❌ | Microsoft's GVFS for huge repos |
| Pack-file delta compression tuning | ❌ | |
| Repository size limits and warnings | ❌ | |
| Background GC / repacking scheduling | ❌ | |
| Forking scalability (object sharing between forks) | ❌ | |

---

## 38. Cloud Development Environments

> GitHub Codespaces, GitLab Web IDE, Gitpod integration — a full category not yet tracked.

| Feature | Status | Notes |
|---------|--------|-------|
| Cloud dev environments (Codespaces-equivalent) | ❌ | Docker-based isolated workspaces |
| Dev container spec support (devcontainer.json) | ❌ | |
| Pre-built environment images | ❌ | |
| Environment lifecycle management (start/stop/timeout) | ❌ | |
| Port forwarding from cloud env | ❌ | |
| Dotfiles personalization support | ❌ | |
| GPU-backed environments | ❌ | |
| Environment billing / usage metering | ❌ | |
| Prebuilt environment snapshots | ❌ | |

---

## 39. AI Coding Assistant (Copilot-equivalent)

> GitHub Copilot, GitLab Duo, and Sourcegraph Cody are now core platform features, not add-ons. Nexus Forge's AGENTS.md describes AI review but not AI code generation at the editor level.

| Feature | Status | Notes |
|---------|--------|-------|
| In-editor AI code completion (LSP-based) | ❌ | |
| AI-generated PR descriptions | ❌ | |
| AI-generated commit messages | ❌ | |
| AI-generated issue summaries | ❌ | |
| AI code explanation (explain this function) | ❌ | |
| AI test generation | ❌ | |
| AI vulnerability fix suggestions | ❌ | |
| AI chat within PR context | ❌ | |
| Natural language to code (prompt-to-PR) | ❌ | |
| AI-generated release notes | ❌ | |
| Nexus AI integration (see AGENTS.md) | 🟡 | Wired in AGENTS.md, not implemented |

---

## 40. Enterprise Identity & Multi-Tenancy

> GitHub Enterprise Managed Users (EMU), GitLab Enterprise Edition auth, and Bitbucket Data Center all have identity models that go far beyond basic auth stubs.

| Feature | Status | Notes |
|---------|--------|-------|
| Enterprise Managed Users (fully IdP-owned identities) | ❌ | |
| Just-in-time (JIT) user provisioning via SAML | ❌ | |
| Nested group / subgroup hierarchy | ❌ | GitLab supports 20 levels deep |
| Cross-org repository access grants | ❌ | |
| Tenant isolation (data plane separation) | ❌ | |
| Bring-your-own-key (BYOK) encryption | ❌ | |
| Customer-managed encryption keys | ❌ | |
| Data residency pinning (region lock) | ❌ | |
| IP allowlist per organization | ❌ | |
| Domain verification for organizations | ❌ | |
| SSO enforcement (block non-SSO access) | ❌ | |
| Audit log streaming (to SIEM) | ❌ | |
| Emergency access / break-glass accounts | ❌ | |

---

## 41. Mobile Apps & Native Clients

> GitHub, GitLab, and Bitbucket all have native mobile apps. This is a separate product vertical.

| Feature | Status | Notes |
|---------|--------|-------|
| iOS native app | ❌ | |
| Android native app | ❌ | |
| Mobile PR review (approve, comment) | ❌ | |
| Mobile issue management | ❌ | |
| Mobile notifications (push) | ❌ | |
| Desktop app (Electron / Tauri) | ❌ | |
| Forge CLI (`nexus` CLI tool) | ❌ | |
| VS Code extension | ❌ | |
| JetBrains plugin | ❌ | |
| Neovim / Emacs plugin | ❌ | |

---

## 42. Community & Social Layer

> GitHub's social graph, Sponsors, Stars, Trending, and Discussions are what made it the dominant platform. Not just features — network effects.

| Feature | Status | Notes |
|---------|--------|-------|
| Repository starring / bookmarking | ❌ | |
| Repository watching (notification subscribe) | ❌ | |
| Public activity feed / social timeline | ❌ | |
| Trending repositories | ❌ | |
| Repository topics / tags | ❌ | |
| GitHub Sponsors / creator funding | ❌ | |
| Public profile pages | ❌ | |
| Contribution streak / activity graph | ❌ | |
| Discussions (forum-style, separate from issues) | ❌ | |
| Q&A discussion categories | ❌ | |
| Discussion pinning and locking | ❌ | |
| Community health files (CODE_OF_CONDUCT, FUNDING.yml) | ❌ | |
| Community standards checklist | ❌ | |
| Open source license detection | ❌ | |

---

## 43. Hosting Models & Deployment Variants

> GitHub offers GitHub.com + GitHub Enterprise Server + GitHub Enterprise Cloud + GitHub AE. GitLab has GitLab.com + GitLab Self-Managed + GitLab Dedicated. This is a product architecture concern, not just a feature.

| Feature | Status | Notes |
|---------|--------|-------|
| SaaS multi-tenant (cloud.nexusforge.io) | ❌ | |
| Self-managed single-tenant install | 🟡 | Docker exists but not production-hardened |
| Air-gapped / offline install | ❌ | |
| High availability (HA) configuration | ❌ | |
| Geo-replication (read-replica regions) | ❌ | GitLab Geo equivalent |
| Satellite sites for remote teams | ❌ | |
| Disaster recovery site | ❌ | |
| Zero-downtime upgrade path | ❌ | |
| Version migration tooling | ❌ | |
| Data import from GitHub / GitLab | ❌ | |
| Data export for full platform migration | ❌ | |

---

## 44. Education, Classroom & OSS Programs

> GitHub Education and GitHub for Nonprofits drive millions of accounts. GitLab for Education does the same. Often overlooked as a growth channel.

| Feature | Status | Notes |
|---------|--------|-------|
| Classroom / course management | ❌ | |
| Assignment creation and distribution | ❌ | |
| Automated grading hooks | ❌ | |
| Student/teacher role differentiation | ❌ | |
| OSS program tier (free for open source) | ❌ | |
| Nonprofit / education discounts | ❌ | |
| Student starter pack equivalent | ❌ | |
| Learning pathways / guided onboarding | ❌ | |

---

## 🎯 Full Platform Feature Parity — Every Feature From Every Major VCS

> The sections below are a complete audit of what **GitHub, GitLab, Gitea, Bitbucket, Azure DevOps, Sourcehut, Phabricator, and Forgejo** offer that is not yet tracked above. Nexus Forge must offer **all of these** — not as alternatives to other platforms, but as the single platform where a developer never has to go anywhere else.

---

## 45. CI/CD — Full Depth (GitHub Actions + GitLab CI + Azure Pipelines + Bitbucket Pipelines)

> What exists in Section 4 is a high-level stub. This section covers the full depth that mature CI/CD systems expose.

### 45.1 Runner Infrastructure
| Feature | Status | Notes |
|---------|--------|-------|
| CI/CD full-depth parity orchestrator | 🟡 | ci-cd-full-depth.ts |
| Hosted (cloud) runners (Linux, macOS, Windows) | ❌ | GitHub provides ubuntu-latest, macos-latest, windows-latest |
| Self-hosted runner registration & management | ❌ | |
| Runner groups (org-level access control) | ❌ | |
| Runner labels / tags for job targeting | ❌ | |
| Runner autoscaling | ❌ | |
| Docker executor (run jobs in containers) | ❌ | |
| Kubernetes executor | ❌ | |
| Shell executor | ❌ | |
| Custom executor support | ❌ | |
| Runner health monitoring | ❌ | |
| Runner token rotation | ❌ | |
| Job concurrency limits | ❌ | |
| Runner IP allowlisting | ❌ | |

### 45.2 Pipeline Features
| Feature | Status | Notes |
|---------|--------|-------|
| YAML pipeline definition (GitHub Actions syntax) | ❌ | |
| YAML pipeline definition (GitLab CI syntax) | ❌ | |
| Pipeline triggers: push, PR, schedule, manual, API, webhook | ❌ | |
| Path filters (only run on changed paths — monorepo) | ❌ | |
| Reusable workflows / pipeline templates | ❌ | |
| Composite actions / step libraries | ❌ | |
| Matrix strategy (build across multiple versions/OSes) | ❌ | |
| Conditional steps (`if:` expressions) | ❌ | |
| Job dependencies (needs: / stages:) | ❌ | |
| DAG pipeline visualization | ❌ | |
| Pipeline caching (restore/save by key) | ❌ | |
| Artifact upload/download between jobs | ❌ | |
| Pipeline approval gates / manual triggers | ❌ | |
| Environment-scoped secrets | ❌ | |
| OIDC token exchange (keyless auth to AWS/GCP/Azure) | ❌ | |
| Pipeline retry on failure | ❌ | |
| Timeout configuration per job and pipeline | ❌ | |
| Pipeline cancellation on new push | ❌ | |
| Pipeline concurrency groups | ❌ | |
| Step output passing between jobs | ❌ | |
| Service containers (spin up Redis/Postgres for tests) | ❌ | |
| Container image builds within pipeline | ❌ | |
| Multi-platform builds (arm64, amd64) | ❌ | |

### 45.3 AutoDevOps & Zero-Config
| Feature | Status | Notes |
|---------|--------|-------|
| AutoDevOps (auto-detect language, auto-configure pipeline) | ❌ | GitLab feature |
| Build packs support | ❌ | |
| Auto SAST, DAST, SBOM in default pipeline | ❌ | |
| Auto container scanning | ❌ | |
| Auto dependency scanning | ❌ | |
| Auto license compliance | ❌ | |

### 45.4 Review Apps & Preview Environments
| Feature | Status | Notes |
|---------|--------|-------|
| Review apps (dynamic preview per branch/PR) | ❌ | GitLab Review Apps, Vercel-style previews |
| Preview environment URL in PR status | ❌ | |
| Auto-destroy review app on PR close | ❌ | |
| Review app access control | ❌ | |

### 45.5 Deployment Tracking
| Feature | Status | Notes |
|---------|--------|-------|
| Named environments (dev, staging, production) | ❌ | |
| Environment history (all deploys to that env) | ❌ | |
| Active deployment indicator | ❌ | |
| Environment protection rules (who can deploy) | ❌ | |
| Required reviewers per environment | ❌ | |
| Deployment approval workflow | ❌ | |
| Auto-stop environments on schedule | ❌ | |
| Kubernetes environment integration | ❌ | |
| Deployment frequency chart | ❌ | |
| Lead time / change failure rate tracking | ❌ | |

### 45.6 Actions / Plugin Marketplace
| Feature | Status | Notes |
|---------|--------|-------|
| Actions/steps marketplace | ❌ | GitHub has 20,000+ actions |
| Published action versioning | ❌ | |
| Verified creator badges | ❌ | |
| Private / internal action sharing (org-scoped) | ❌ | |
| Required workflow enforcement (org-level) | ❌ | |

---

## 46. Package & Artifact Registries — Full Scope

> Every major platform now includes built-in package hosting. Nexus Forge must host all of them natively.

| Package Format | Status | Platform origin | Notes |
|----------------|--------|-----------------|-------|
| Package registry matrix orchestrator | 🟡 | Nexus Forge | package-registry-matrix.ts |
| npm / yarn / pnpm | ❌ | GitHub, GitLab, Gitea | |
| Docker / OCI container images | ❌ | GitHub, GitLab, Gitea | |
| Maven (Java) | ❌ | GitHub, GitLab, Gitea, Azure | |
| Gradle (Java) | ❌ | GitHub, GitLab | |
| NuGet (.NET) | ❌ | GitHub, GitLab, Gitea, Azure | |
| PyPI (Python) | ❌ | GitHub, GitLab, Gitea | |
| RubyGems | ❌ | GitHub, GitLab, Gitea | |
| Composer (PHP) | ❌ | GitLab, Gitea | |
| Conan (C/C++) | ❌ | GitLab, Gitea | |
| Helm charts | ❌ | GitLab, Gitea | |
| Alpine Linux packages (.apk) | ❌ | Gitea | |
| Debian packages (.deb) | ❌ | Gitea | |
| RPM packages (.rpm) | ❌ | Gitea | |
| Cargo (Rust) | ❌ | GitLab | |
| Go module proxy | ❌ | GitLab | |
| Generic/raw artifact upload | ❌ | All platforms | |
| Package download count / analytics | ❌ | |
| Package immutability (no overwrite of published version) | ❌ | |
| Package retention policies | ❌ | |
| Private package authentication | ❌ | |
| Package access control (public/internal/private) | ❌ | |
| Package deprecation / yanking | ❌ | |
| Package vulnerability alerts | ❌ | |
| Package mirroring (proxy upstream registries) | ❌ | |
| SBOM export per package | ❌ | |

---

## 47. Repository Mirrors & Federation (Gitea / GitLab Mirror)

> Gitea has one of the best mirror systems — push and pull mirrors to/from any remote. This is core to Nexus Forge's multi-VCS story.

| Feature | Status | Notes |
|---------|--------|-------|
| Mirror federation orchestrator | 🟡 | repository-mirror-federation.ts |
| Pull mirror (sync FROM GitHub/GitLab/etc on schedule) | ❌ | Gitea mirrors |
| Push mirror (sync TO GitHub/GitLab/etc on push) | ❌ | |
| Mirror force-push | ❌ | |
| Mirror sync interval configuration | ❌ | |
| Mirror sync status and error reporting | ❌ | |
| Fork sync from upstream | ❌ | |
| Migration import (from GitHub, GitLab, Gitea, Bitbucket, Gogs) | ❌ | Gitea Migration API |
| Migration with issues, PRs, comments, labels, milestones | ❌ | |
| Migration with wiki | ❌ | |
| Migration with releases | ❌ | |
| Migration progress tracking | ❌ | |
| Repository export bundle (git bundle format) | ❌ | |
| Gitea → Forgejo compatibility | ❌ | |

---

## 48. Snippets, Gists & Pastes

> Every major platform has a code snippet / paste feature. Currently not tracked at all.

| Feature | Status | Notes |
|---------|--------|-------|
| Snippets/gists parity orchestrator | 🟡 | snippets-gists.ts |
| Create snippet / gist (single or multi-file) | ❌ | GitHub Gists, GitLab Snippets, Gitea Gists |
| Public / secret / internal snippets | ❌ | |
| Syntax highlighting in snippets | ❌ | |
| Snippet versioning (full git history) | ❌ | |
| Snippet embedding (embed in external pages) | ❌ | |
| Snippet forking | ❌ | |
| Snippet commenting | ❌ | |
| Snippet search | ❌ | |
| Snippet expiry / self-destruct | ❌ | |
| Snippet collections / personal library | ❌ | |
| Raw snippet access (curl-friendly URL) | ❌ | |
| Gist API compatibility (GitHub API compat) | ❌ | |

---

## 49. Static Site Hosting (GitHub Pages / GitLab Pages / Gitea Pages)

| Feature | Status | Notes |
|---------|--------|-------|
| Pages hosting parity orchestrator | 🟡 | pages-hosting.ts |
| Static site hosting from repo branch | ❌ | GitHub Pages, GitLab Pages |
| Custom domain with HTTPS (Let's Encrypt) | ❌ | |
| Jekyll / Hugo / 11ty / any SSG support | ❌ | |
| Deploy from CI pipeline | ❌ | |
| Per-project Pages site | ❌ | |
| Org / user Pages site (username.nexusforge.io) | ❌ | |
| Access control (public / authenticated-only) | ❌ | |
| Pages redirects and custom 404 | ❌ | |
| Pages deployment history | ❌ | |
| Pages bandwidth tracking | ❌ | |

---

## 50. Issue Management — Full Depth (GitLab + GitHub + Azure Boards)

> Section 3 covers the surface. This covers the full feature set GitLab and GitHub Issues expose.

### 50.1 Issue Fields & Metadata
| Feature | Status | Notes |
|---------|--------|-------|
| Issue management full-depth orchestrator | 🟡 | issue-management-full-depth.ts |
| Issue types (bug, feature, task, incident, test case) | ❌ | GitLab Issue types |
| Issue weight / story points | ❌ | GitLab |
| Issue health status (on track, needs attention, at risk) | ❌ | GitLab |
| Issue due dates | ❌ | |
| Issue start date | ❌ | |
| Time tracking (estimate + spend) | ❌ | GitLab `/estimate`, `/spend` commands |
| Time tracking reports | ❌ | |
| Issue severity levels | ❌ | |
| Issue priority levels | ❌ | |
| Custom issue fields | ❌ | GitHub Projects custom fields |
| Issue pinning (to top of list) | ❌ | |
| Issue locking (disable new comments) | ❌ | |
| Confidential issues (visible to team only) | ❌ | GitLab |
| Issue tasks / checklists (markdown task lists) | ❌ | |
| Issue child issues / sub-tasks | ❌ | GitHub Sub-issues |
| Issue relationships (blocks, is blocked by, relates to, clones) | ❌ | GitLab |
| Issue participants list | ❌ | |
| Issue reactions / emoji | ❌ | |

### 50.2 Epics & Hierarchy
| Feature | Status | Notes |
|---------|--------|-------|
| Epics (group issues across milestones) | ❌ | GitLab Epics |
| Epic → Issue hierarchy | ❌ | |
| Multi-level hierarchy (Initiative → Epic → Issue → Task) | ❌ | |
| Epic roadmap timeline | ❌ | |
| Epic health rollup | ❌ | |
| Epic labels and milestones | ❌ | |
| Cross-project epics | ❌ | GitLab group-level epics |

### 50.3 Iterations & Sprints
| Feature | Status | Notes |
|---------|--------|-------|
| Iterations / sprints (time-boxed work periods) | ❌ | GitLab Iterations |
| Automated iteration cadence | ❌ | |
| Iteration scope (issues assigned to iteration) | ❌ | |
| Iteration burndown chart | ❌ | |
| Iteration velocity chart | ❌ | |
| Iteration retrospective notes | ❌ | |

### 50.4 Automation & Quick Actions
| Feature | Status | Notes |
|---------|--------|-------|
| Quick actions (slash commands in comments: /assign, /close, /label) | ❌ | GitLab, GitHub |
| Issue automation rules (auto-close, auto-label, auto-assign) | ❌ | |
| Issue templates with pre-populated fields | ❌ | |
| Issue forms (structured YAML templates) | ❌ | GitHub Issue Forms |
| Auto-close issue on PR merge (fixes #123) | ❌ | |
| Issue triage queue | ❌ | |
| Issue SLA / response time targets | ❌ | |
| Issue aging alerts | ❌ | |

---

## 51. Project Boards — Full Depth (GitHub Projects v2 + GitLab Boards + Azure Boards)

| Feature | Status | Notes |
|---------|--------|-------|
| Project boards full-depth orchestrator | 🟡 | project-boards-full-depth.ts |
| Multiple boards per project | ❌ | |
| Board columns (customizable) | ❌ | |
| WIP limits per column | ❌ | |
| Swimlanes (group by assignee, label, epic) | ❌ | |
| Auto-move cards on status change | ❌ | GitHub automation |
| Filtered views (by assignee, label, milestone) | ❌ | |
| Table view (spreadsheet-style issue management) | ❌ | GitHub Projects v2 |
| Roadmap / timeline view (Gantt-style) | ❌ | GitHub Projects v2 |
| Custom fields on board items (status, priority, size) | ❌ | |
| Field sum aggregation (total story points) | ❌ | |
| Board workflow triggers | ❌ | |
| Group-level / org-level boards (cross-repo) | ❌ | GitLab group boards |
| Portfolio boards (cross-project roll-up) | ❌ | |
| Dependency lines on roadmap | ❌ | |
| Azure Work Items (full work item type system) | ❌ | Azure DevOps |
| Queries (saved issue filters, Azure-style) | ❌ | Azure Boards |
| Delivery plans (multi-team timeline) | ❌ | Azure DevOps |

---

## 52. Test & Quality Management (GitLab Quality / Azure Test Plans)

> Entire product area not yet tracked — Azure DevOps has Test Plans as a standalone product.

| Feature | Status | Notes |
|---------|--------|-------|
| Test quality management orchestrator | 🟡 | test-quality-management.ts |
| Test cases (managed test definitions) | ❌ | GitLab Quality Management, Azure Test Plans |
| Test suites (grouped test cases) | ❌ | |
| Test plans (a run of test suites) | ❌ | |
| Manual test execution UI | ❌ | |
| Test run results recording | ❌ | |
| Test case versioning | ❌ | |
| Test result analytics | ❌ | |
| Test coverage visualization in MR/PR | ❌ | GitLab |
| Code coverage enforcement (block merge below threshold) | ❌ | |
| Test flakiness detection and quarantine | ❌ | |
| Browser performance testing in CI | ❌ | GitLab |
| Load performance testing in CI | ❌ | GitLab |
| Accessibility testing (axe) in CI | ❌ | GitLab |
| Traceability (requirements ↔ test cases ↔ issues) | ❌ | GitLab |
| Exploratory testing sessions | ❌ | Azure Test Plans |

---

## 53. Requirements Management (GitLab Requirements / Azure Boards)

| Feature | Status | Notes |
|---------|--------|-------|
| Requirements management orchestrator | 🟡 | requirements-management.ts |
| Requirements as first-class objects | ❌ | GitLab Requirements |
| Requirement status (open, satisfied, missed) | ❌ | |
| Requirement ↔ test case traceability | ❌ | |
| Requirement ↔ issue traceability | ❌ | |
| Import requirements from CSV | ❌ | |
| Compliance traceability matrix export | ❌ | |

---

## 54. Design & Asset Management (GitLab Design Management)

> Designers live on Figma and Zeplin. GitLab brings design review into the development workflow.

| Feature | Status | Notes |
|---------|--------|-------|
| Design asset management orchestrator | 🟡 | design-asset-management.ts |
| Upload design files to issues (PNG, Sketch, Figma link) | ❌ | GitLab Design Management |
| Design version history | ❌ | |
| Design diff viewer (overlay before/after) | ❌ | |
| Pin comments on specific areas of design | ❌ | |
| Design review status | ❌ | |
| Figma embed in issues/MRs | ❌ | |
| Lottie / video asset previews | ❌ | |

---

## 55. Service Desk & External Issue Intake (GitLab Service Desk)

> GitLab Service Desk turns a repo into a customer support inbox — emails become issues automatically.

| Feature | Status | Notes |
|---------|--------|-------|
| Incoming email → issue creation | ❌ | GitLab Service Desk |
| Service desk email address per project | ❌ | |
| Response templates (canned replies) | ❌ | |
| Customer email reply via issue comments | ❌ | |
| Service desk SLA policies | ❌ | |
| Service desk metrics dashboard | ❌ | |
| Customer satisfaction surveys | ❌ | |
| Integration with Zendesk / Freshdesk | ❌ | |

---

## 56. Code Intelligence & Navigation (Sourcegraph / GitHub Code Search)

> GitHub acquired Sourcegraph-style functionality. GitLab integrates with it. This is a separate product category.

| Feature | Status | Notes |
|---------|--------|-------|
| Precise code navigation (go-to-definition cross-repo) | ❌ | |
| Cross-repository reference search | ❌ | |
| Symbol search (find all usages of a function org-wide) | ❌ | |
| Dependency usage search (where is this package used) | ❌ | |
| Code intelligence via SCIP / LSIF index | ❌ | |
| Regular expression code search (GitHub new code search) | ❌ | |
| Saved searches | ❌ | |
| Search across all branches / tags | ❌ | |
| Commit search by date range, author, message | ❌ | |
| Code ownership heatmap | ❌ | |

---

## 57. Wiki — Full Depth (GitHub Wiki / GitLab Wiki / Gitea Wiki)

| Feature | Status | Notes |
|---------|--------|-------|
| Per-repository wiki | ❌ | |
| Group / organization wiki | ❌ | GitLab group wiki |
| Markdown + Asciidoc + reStructuredText support | ❌ | |
| Wiki history (full git history) | ❌ | Wikis are git repos |
| Wiki page creation / editing in browser | ❌ | |
| Wiki table of contents | ❌ | |
| Wiki sidebar navigation | ❌ | |
| Wiki search | ❌ | |
| Wiki clone (edit locally via git) | ❌ | |
| Wiki access control (who can edit) | ❌ | |
| Wiki templates | ❌ | |

---

## 58. Notifications & Activity Feeds — Full Depth

| Feature | Status | Notes |
|---------|--------|-------|
| Per-event notification preferences (granular) | ❌ | |
| Web notification inbox (GitHub-style notification center) | ❌ | |
| Mark as read / done / saved | ❌ | |
| Notification filters by repo / org / type | ❌ | |
| Watch / unwatch individual repositories | ❌ | |
| Watch levels: all activity, just mentions, security only | ❌ | |
| Email notification digest | ❌ | |
| Email notification threading (reply in email = comment) | ❌ | GitLab email reply |
| Slack integration (deep: post per-event) | ❌ | |
| Microsoft Teams integration | ❌ | |
| Discord integration | ❌ | |
| PagerDuty / Opsgenie integration | ❌ | |
| In-app notification feed / activity timeline | ❌ | |

---

## 59. Branch & Merge Rules — Full Depth (Repository Rulesets / Protected Branches)

| Feature | Status | Notes |
|---------|--------|-------|
| Repository rulesets (GitHub new model — layered rules) | ❌ | GitHub Rulesets |
| Push restrictions (who can push to branch) | ❌ | |
| Force push restriction | ❌ | |
| Deletion restriction | ❌ | |
| Require pull request before merging | ❌ | |
| Required approvals count | ❌ | |
| Required approvals from code owners | ❌ | |
| Dismiss stale reviews on new push | ❌ | |
| Require conversation resolution before merge | ❌ | |
| Require signed commits | ❌ | |
| Require linear history | ❌ | |
| Required status checks (named CI jobs must pass) | ❌ | |
| Bypass list (specific users/roles can override) | ❌ | |
| Merge request approval rules (GitLab model) | ❌ | GitLab approval rules |
| Approval rule groups (any member of Security team) | ❌ | |
| Approval forwarding (delegating approval) | ❌ | |
| Merge trains (sequential validation) | ❌ | GitLab |
| External status checks (third-party must green) | ❌ | GitLab |
| Protected tag patterns | ❌ | |

---

## 60. Releases & Release Management — Full Depth

| Feature | Status | Notes |
|---------|--------|-------|
| Create release from tag | ❌ | |
| Release description (Markdown) | ❌ | |
| Asset upload to release (binaries, checksums) | ❌ | |
| Asset link (point to S3/CDN rather than host) | ❌ | GitLab |
| Release permalink (latest) | ❌ | |
| Release API | ❌ | |
| Release webhook events | ❌ | |
| Auto-generated release notes (from merged PRs since last tag) | ❌ | GitHub |
| Release channels (stable, beta, nightly, LTS) | ❌ | |
| Release comparison (diff between two releases) | ❌ | |
| Release subscriptions (notify me of new releases) | ❌ | |
| Release download count tracking | ❌ | |
| Pre-release flag | ❌ | |
| Release evidences (GitLab — snapshot of CI job reports, linked issues) | ❌ | GitLab |

---

## 61. Kubernetes & Cloud Native Integration

| Feature | Status | Notes |
|---------|--------|-------|
| Kubernetes cluster connection (Agent for Kubernetes) | ❌ | GitLab Agent |
| GitOps workflow (pull-based deploy from repo) | ❌ | |
| Kubernetes environment dashboard | ❌ | |
| Pod logs in GitLab/platform UI | ❌ | |
| Kubernetes deployment status in MR | ❌ | |
| Helm release tracking | ❌ | |
| Terraform state management | ❌ | GitLab managed Terraform |
| Terraform plan output in MR | ❌ | |
| Infrastructure as Code linting in CI | ❌ | |
| Pulumi / CDK integration | ❌ | |

---

## 62. Error Tracking & Application Monitoring Integration

> GitLab has built-in error tracking. GitHub integrates with Sentry. Both platforms connect code → production errors.

| Feature | Status | Notes |
|---------|--------|-------|
| Error tracking inbox (Sentry-compatible) | ❌ | GitLab Error Tracking |
| Link error to issue automatically | ❌ | |
| Error fingerprinting / grouping | ❌ | |
| Error frequency and timeline | ❌ | |
| Affected release detection | ❌ | |
| Resolve error from commit | ❌ | |
| Feature flag evaluation in error context | ❌ | |
| Custom alert rules | ❌ | |
| Alert ↔ incident linking | ❌ | |

---

## 63. Feature Flags — Native (GitLab Feature Flags / Unleash-compatible)

> GitLab has built-in feature flags, not just as stubs — as a full delivery mechanism tied to deployments and rollouts.

| Feature | Status | Notes |
|---------|--------|-------|
| Feature flag creation (boolean, gradual, user-list, percentage) | ❌ | GitLab Feature Flags, Unleash |
| Strategy types: all users, % rollout, user ID list, GitLab user list | ❌ | |
| Environment-scoped flags | ❌ | |
| Flag evaluation API (for client SDKs) | ❌ | |
| SDKs: JavaScript, Python, Go, Ruby, Java | ❌ | |
| Flag audit log | ❌ | |
| Flag linked to issue / MR | ❌ | |
| Flag stale detection (clean up old flags) | ❌ | |
| A/B test integration | ❌ | |
| Flag kill switch (emergency disable) | ❌ | |

---

## 64. Integrations & Webhooks — Full Depth

### 64.1 Deep Platform Integrations
| Feature | Status | Notes |
|---------|--------|-------|
| Jira integration (link issues, auto-transition, smart commits) | ❌ | GitHub + GitLab + Bitbucket all have deep Jira |
| Confluence integration | ❌ | |
| Trello integration | ❌ | Bitbucket native |
| Linear integration | ❌ | |
| Asana integration | ❌ | |
| Monday.com integration | ❌ | |
| Slack integration (deep: slash commands, notifications, approvals) | ❌ | |
| Microsoft Teams integration | ❌ | |
| Datadog integration | ❌ | |
| New Relic integration | ❌ | |
| PagerDuty integration | ❌ | |
| Splunk integration | ❌ | |
| Terraform Cloud integration | ❌ | |
| Vault (HashiCorp) integration | ❌ | |
| Artifactory integration | ❌ | |
| SonarQube integration | ❌ | |
| Snyk integration | ❌ | |
| Checkmarx / Veracode integration | ❌ | |
| AWS CodeDeploy / CodeBuild integration | ❌ | |
| Google Cloud Build integration | ❌ | |
| Azure Pipelines integration | ❌ | |

### 64.2 OAuth2 Provider (Gitea / GitLab as identity provider)
| Feature | Status | Notes |
|---------|--------|-------|
| Nexus Forge as OAuth2 provider | ❌ | Let other tools log in using Nexus Forge |
| OAuth2 app registration | ❌ | |
| OAuth2 scopes management | ❌ | |
| OAuth2 token introspection | ❌ | |
| OIDC discovery document | ❌ | |

### 64.3 Webhook System Deep
| Feature | Status | Notes |
|---------|--------|-------|
| Repo-level webhooks | ❌ | |
| Org-level webhooks | ❌ | |
| Global admin webhooks | ❌ | |
| Webhook event filtering (choose which events) | ❌ | |
| Webhook delivery logs (success/failure history) | ❌ | |
| Webhook secret verification (HMAC-SHA256) | ❌ | |
| Webhook retry with exponential backoff | ❌ | |
| Webhook payload inspection | ❌ | |
| Webhook redeliver (replay a past delivery) | ❌ | |
| Webhook SSL verification toggle | ❌ | |

---

## 65. Enterprise Self-Hosted Administration

> GitLab has an extensive admin panel. Gitea has a comprehensive site admin. This is what instance operators need.

| Feature | Status | Notes |
|---------|--------|-------|
| Site admin dashboard | ❌ | |
| Global user management (create/suspend/delete/impersonate) | ❌ | |
| Global organization management | ❌ | |
| Global repository overview | ❌ | |
| Instance-level statistics (users, repos, traffic) | ❌ | |
| Instance announcement banners | ❌ | |
| Instance-level maintenance mode | ❌ | |
| Custom landing page / branding | ❌ | |
| Custom email templates | ❌ | |
| Custom syntax highlighting themes | ❌ | |
| Instance-level webhook | ❌ | |
| Admin audit log | ❌ | |
| Admin panel for runner management | ❌ | |
| Storage usage per user/org | ❌ | |
| Quota enforcement | ❌ | |
| Repo size limits | ❌ | |
| Register token management (for runners) | ❌ | |
| Background job queue viewer | ❌ | |
| Database maintenance tools | ❌ | |
| Instance backup and restore from admin UI | ❌ | |
| Monitoring dashboard (health, queue depth) | ❌ | |
| LDAP integration for self-hosted | ❌ | |
| Two-factor enforcement globally | ❌ | |
| Allowed email domain restriction | ❌ | |
| Sign-up restrictions (invite only, domain allowlist) | ❌ | |
| Abuse report handling | ❌ | |
| DMCA / content takedown tooling | ❌ | |

---

## 66. Sourcehut Features (sr.ht — The Minimalist Benchmark)

> Sourcehut is the most minimalist production forge — everything is email and plain text based. It demonstrates what a forge can be with extreme simplicity.

| Feature | Status | Notes |
|---------|--------|-------|
| Mailing list hosting (patches via email) | ❌ | sr.ht/lists |
| Email-based code review (patch submission by email) | ❌ | |
| Todo lists (sr.ht/todo) | ❌ | |
| Build service (sr.ht/builds — YAML-based) | ❌ | |
| Pages hosting (sr.ht/pages) | ❌ | |
| Paste service (sr.ht/paste) | ❌ | |
| No JavaScript fallback / accessibility mode | ❌ | |
| API-first design (everything via API) | ❌ | sr.ht is API-first |

---

## 67. Phabricator / Phacility Features

> Phabricator was ahead of its time. Some features it had in 2012 that others still haven't caught up on.

| Feature | Status | Notes |
|---------|--------|-------|
| Differential (structured code review with revisions) | ❌ | |
| Revision lifecycle (needs review → accepted → landed) | ❌ | |
| Inline comment collapsing | ❌ | |
| Maniphest (task tracker with custom workflows) | ❌ | |
| Phriction (wiki with structured content) | ❌ | |
| Phame (developer blog platform) | ❌ | |
| Herald (automation rules: if X then Y) | ❌ | Extremely powerful rule engine |
| Owners (CODEOWNERS equivalent) | ❌ | |
| Audit (commit-level mandatory review) | ❌ | Phabricator audits |
| Paste (pastebin) | ❌ | |
| Projects as tags (cross-cutting concerns) | ❌ | |
| Workboards (kanban on Projects) | ❌ | |

---

## 68. Developer Profile & Career Features (GitHub Profile++)

| Feature | Status | Notes |
|---------|--------|-------|
| Developer public profile page | ❌ | |
| Profile README (custom Markdown displayed on profile) | ❌ | GitHub profile README |
| Contribution activity graph (green squares) | ❌ | |
| Pinned repositories | ❌ | |
| Contribution streak | ❌ | |
| Achievement badges | ❌ | |
| Trophies / stats cards | ❌ | Community extensions |
| Skills / language breakdown | ❌ | |
| Followers / following social graph | ❌ | |
| Hire me / available for work flag | ❌ | |
| Resume export from profile | ❌ | |
| Portfolio view (best projects highlighted) | ❌ | |

---

## 69. Sponsorship & Funding (GitHub Sponsors)

| Feature | Status | Notes |
|---------|--------|-------|
| Developer / org sponsorship tiers | ❌ | GitHub Sponsors |
| Recurring and one-time sponsorships | ❌ | |
| Sponsor-only content / repos | ❌ | |
| FUNDING.yml (link to Patreon/Open Collective/Ko-fi) | ❌ | |
| Sponsorship dashboard for creators | ❌ | |
| Stripe / payment gateway integration | ❌ | |
| Payout processing | ❌ | |
| Sponsor recognition in README | ❌ | |

---

## 70. Accessibility, Internationalization & Localization

| Feature | Status | Notes |
|---------|--------|-------|
| Full keyboard navigation | ❌ | |
| Screen reader support (ARIA labels) | ❌ | |
| WCAG 2.1 AA compliance | ❌ | |
| High contrast mode | ❌ | |
| Font size / zoom support | ❌ | |
| Right-to-left (RTL) layout support | ❌ | |
| Interface localization (i18n) | ❌ | Gitea supports 40+ languages |
| Community translation platform | ❌ | |
| Locale-aware date/time display | ❌ | |
| Locale-aware number formatting | ❌ | |

---

## 71. Depth Parity Backlog (Sub-Options, Edge Cases, API Variants)

> Yes, this layer is equally important. Platform parity fails if 90% of a workflow exists but the last 10% (the edge behavior teams rely on) is missing.

| Depth Capability | Status | Notes |
|------------------|--------|-------|
| Inventory stub engine orchestration | 🟡 | inventory-stub-engine.ts |
| API behavior parity matrix per endpoint (GitHub/GitLab/Gitea/Bitbucket/Azure) | ❌ | Track request/response semantics, not only endpoint names |
| Pagination parity (`page`, `per_page`, cursors, sort defaults, max limits) | ❌ | |
| Filter parity (labels, assignee, state, date ranges, search operators) | ❌ | |
| Permission edge-case parity (owner, maintainer, guest, external collaborator) | ❌ | |
| Webhook payload parity per event version | ❌ | |
| Error-code parity (`403` vs `404` concealment rules) | ❌ | Security-sensitive behavior |
| Rate-limit behavior parity (headers, reset windows, secondary limits) | ❌ | |
| Merge policy edge parity (stale review dismissal, required conversations resolved) | ❌ | |
| Migration fidelity parity (preserve users, timestamps, threaded comments, reactions) | ❌ | |
| Markdown rendering parity (task lists, mentions, emoji, diagrams, math) | ❌ | |
| Search syntax parity (qualifiers, boolean ops, regex modes, path scopes) | ❌ | |
| Audit/event vocabulary parity (normalized cross-platform event taxonomy) | ❌ | |
| Backward-compatibility policy by API version | ❌ | |
| SDK parity for major languages (TS, Python, Go, Java, Rust) | ❌ | |
| Terraform/provider parity for infra automation | ❌ | |
| CLI compatibility layer (`gh`/`glab` style command mapping) | ❌ | |
| Import/export round-trip parity tests | ❌ | No data loss across migration cycles |
| Contract test suite against live reference platforms | ❌ | Golden test corpus for parity assertions |

---

## 72. Platform Coverage Expansion (Beyond GitLab)

> This strategy is not GitLab-only. Nexus Forge must absorb the useful feature surface of all meaningful forge ecosystems.

| Platform Family | Status | Coverage Goal |
|-----------------|--------|---------------|
| GitHub | 🟡 | Complete parity + migration + API compatibility layers |
| GitLab | 🟡 | Complete parity + migration + API compatibility layers |
| Gitea / Forgejo | 🟡 | Complete parity + mirror and federation parity |
| Bitbucket Cloud / Data Center | ❌ | Add full parity matrix |
| Azure DevOps (Repos, Boards, Pipelines, Artifacts, Test Plans) | ❌ | Add full parity matrix |
| SourceForge | ❌ | Add repo hosting, tickets, file release mirrors, project pages parity |
| Sourcehut | 🟡 | Add email-first workflows parity |
| Gerrit | ❌ | Add patchset and review-label workflow parity |
| Perforce Helix (enterprise bridges) | ❌ | Add hybrid mono-repo and large binary workflow bridges |
| AWS CodeCommit / CodePipeline ecosystem | ❌ | Add migration and integration parity |
| Launchpad / Savannah / legacy forges | ❌ | Add migration-grade compatibility where demanded |

---

## 73. Parity Acceptance Criteria (Definition Of Done)

| Criterion | Status | Target |
|----------|--------|--------|
| Feature existence parity | ❌ | Every mapped capability has a Nexus Forge implementation |
| Behavior parity | ❌ | 95%+ contract test pass rate versus reference platforms |
| Migration fidelity | ❌ | 99.9%+ data fidelity for issues/PRs/comments/releases/wiki |
| API compatibility | ❌ | Stable compatibility layer for major ecosystem clients |
| Workflow parity | ❌ | Developers can complete day-to-day flows with zero fallback to old platform |
| Scale parity | ❌ | SLOs match enterprise expectations for large orgs/repos |
| Admin parity | ❌ | Self-hosted and SaaS operators get complete control surfaces |
| Security/compliance parity | ❌ | Meets baseline enterprise controls (SOC2/ISO/GDPR workflows) |

---

## What "Production Ready" Actually Requires

To be honest about the path from here:

| Milestone | What it means |
|-----------|---------------|
| **Feature-Stubbed** | All ❌ features become 🟡 stubs. The platform has a consistent, navigable UI surface for every capability. *(Current goal)* |
| **Feature-Complete** | All 🟡 stubs become ✅ implementations. Every route does real work backed by real storage. |
| **Infrastructure-Complete** | Sections 21-29 are done. Real protocols, real DB, real queues, real TLS. |
| **Quality-Complete** | Section 30 done. Test coverage ≥80%, load tests pass at target RPS. |
| **Frontend-Complete** | Section 31 done. Real auth flows, diff viewer, file browser, accessibility, mobile. |
| **Compliance-Ready** | Sections 32-33 done. API spec, legal docs, GDPR operational. |
| **Platform Parity** | Sections 35-70 done. Every feature GitHub + GitLab + Gitea + Bitbucket + Azure DevOps offers. |
| **Depth-Parity Complete** | Sections 71-73 done. Sub-options, edge behavior, and API/workflow compatibility are validated via contract tests. |
| **Platform Superiority** | Sections 35-70 done *plus* Nexus Forge-native capabilities no other platform has: unified multi-VCS federation, cross-platform PR federation, unified identity across VCS backends, cross-platform code search, unified pipeline across backends. |
| **The Powerhouse** | Developers never need to leave. Every tool, every workflow, every team size, every use case. Self-hosted, cloud, or federated. |

---

## Platform Parity Reality Check

| Platform | Feature Scope | Nexus Forge Parity Gap |
|----------|---------------|------------------------|
| **GitHub** | ~1,500 features, 700+ API endpoints | ~80% not yet tracked |
| **GitLab** | ~2,000 features (self-reported) | ~75% not yet tracked |
| **Gitea / Forgejo** | ~800 features | ~60% not yet tracked |
| **Bitbucket** | ~700 features | ~70% not yet tracked |
| **Azure DevOps** | ~1,200 features (5 product areas) | ~85% not yet tracked |
| **SourceForge** | legacy but still meaningful OSS hosting workflows | not yet fully tracked |
| **Sourcehut** | ~200 features | ~50% not yet tracked |
| **Combined unique** | **~4,500+ distinct capabilities** | tracked map now includes depth parity requirements |

> **The goal is not to be at 15% and call it done. The goal is to close that gap until a developer can live entirely on Nexus Forge and lose nothing compared to wherever they came from.**

---

## Updated Summary Counts

| Category | Status | Exact Count |
|----------|--------|-------------|
| ✅ IMPLEMENTED | Working, persisted, tested | **17** |
| 🟡 STUB | Route + mock response + UI | **195** |
| ❌ NOT YET | Tracked but not yet built | **922** |
| ❌ PRODUCTION INFRA (Sections 21-34) | Platform plumbing | included in 922 |
| ❌ PLATFORM PARITY (Sections 35-70) | Cross-platform deep features audit | included in 922 |
| ❌ DEPTH PARITY (Sections 71-73) | Sub-options, edge behavior, compatibility variants | included in 922 |
| **Total Tracked** | | **1,134** |

> The document tracks a four-layer roadmap: implemented features, stubs, missing core features, and depth parity. Exact totals are now 1,134 tracked features: 17 implemented, 195 stubs, and 922 not yet built. The depth layer (Sections 71-73) is treated as a first-class requirement, not optional cleanup.

---

*This document is auto-maintained as part of Nexus Forge development. Update status fields as features progress from ❌ → 🟡 → ✅.*
