# Nexus Forge — Agent Patterns & AI Integration

This document describes how **Nexus AI** and agent-based automation integrate with Nexus Forge.

## Integration Points

### 1. Code Review on PR Creation

When a PR is opened:

```
User: Create PR from branch feature/login
  ↓
Webhook: POST /api/webhooks/pr-created { pr_id, from_branch, to_branch }
  ↓
Manager: Queue { type: "code_review", pr_id }
  ↓
Agent: Poll queue, fetch PR diff
  ↓
Agent: Call NEXUS_AI_URL/v1/chat/completions
  {
    model: "nexus-ai/*",
    messages: [
      { role: "system", content: "You are a code reviewer. Focus on security, performance, and maintainability." },
      { role: "user", content: "Review this diff:\n\n{diff}" }
    ]
  }
  ↓
Ensemble: Multiple models vote on quality
  ↓
Update PR: Add review comment with suggestions
```

### 2. Issue → PR Generation

When an issue is created and `GENERATE_ON_ISSUE=true`:

```
User: File issue "Add password validation"
  ↓
Webhook: POST /api/webhooks/issue-created { issue_id, title, description }
  ↓
Agent: Poll queue, fetch issue details
  ↓
Agent: Call NEXUS_AI_URL/v1/chat/completions
  {
    model: "nexus-ai/*",
    messages: [
      { role: "system", content: "You are a code generator. Create working, tested code." },
      { role: "user", content: "Implement: {issue_description}\n\nRepository: {repo_path}\nLanguage: {language}" }
    ]
  }
  ↓
Agent: Receives code suggestion
  ↓
Agent: Create commit + PR with suggested code
  ↓
Mark issue: "PR #123 generated — awaiting review"
```

### 3. Pre-commit Hooks (Client-side)

When user runs `git push`:

```
Client: git push origin feature/xyz
  ↓
Server: Receive pack refs
  ↓
Hook: Pre-commit validation
  - Lint check (ESLint, clippy, etc.)
  - Type check (TypeScript, mypy, etc.)
  - Security scan (Trivy, semgrep)
  ↓
Fail: Reject push, explain errors
  or
Success: Accept push, log activity
```

### 4. CI/CD: Auto-Deploy on Merge

When a PR is merged to `main`:

```
User: Merge PR #123 to main
  ↓
Webhook: POST /api/webhooks/merge { branch: "main", commit_hash }
  ↓
Check: NEXUS_DEPLOY_URL configured?
  Yes ↓
Agent: POST NEXUS_DEPLOY_URL/deploy
  {
    repo: "my-project",
    ref: "main",
    commit: "abc123...",
    trigger: "forge-webhook"
  }
  ↓
Deploy: Trigger build + deploy pipeline
  ↓
Update PR: Add deploy status (in-progress → success/failed)
```

## Configuration

### Environment Variables

```bash
# Nexus AI integration
NEXUS_AI_URL=http://localhost:8000
NEXUS_AI_API_KEY=sk-...          # Optional: auth token if AI service requires it

# Deployment integration
NEXUS_DEPLOY_URL=http://localhost:3000
NEXUS_DEPLOY_TOKEN=token-xxx

# Agent behavior
GENERATE_ON_ISSUE=false           # Auto-generate PRs from issues
AUTO_REVIEW_ON_PR=true            # Auto-review PRs with AI
PRE_COMMIT_CHECKS=true            # Validate code before accepting push
```

### Per-Repository Settings

```json
{
  "repoName": "my-project",
  "ai": {
    "enabled": true,
    "reviewModel": "nexus-ai/ensemble",
    "securityFocus": true,
    "autoFixMinor": true
  },
  "deployment": {
    "enabled": true,
    "branch": "main",
    "provider": "nexus-deploy"
  }
}
```

## Agent Task Queue

Internal queue for background agent work:

```typescript
interface AgentTask {
  id: string;
  type: "code_review" | "generate_code" | "lint_check" | "deploy" | "sync_federation";
  repo_id: number;
  pr_id?: number;
  issue_id?: number;
  payload: Record<string, any>;
  status: "queued" | "in-progress" | "completed" | "failed";
  created_at: string;
  started_at?: string;
  completed_at?: string;
}
```

### Queue Storage

SQLite table:
```sql
CREATE TABLE agent_tasks (
  id TEXT PRIMARY KEY,
  type TEXT NOT NULL,
  repo_id INTEGER NOT NULL,
  pr_id INTEGER,
  issue_id INTEGER,
  payload TEXT NOT NULL,
  status TEXT NOT NULL,
  created_at TEXT DEFAULT CURRENT_TIMESTAMP,
  started_at TEXT,
  completed_at TEXT,
  FOREIGN KEY (repo_id) REFERENCES repositories(id)
);
```

## Safety & Guardrails

### Code Review Safety

- ✅ Read-only suggestions (no auto-commit without human approval)
- ✅ Ensemble voting (avoid single-model bias)
- ✅ Fallback: If AI service unavailable, log warning but don't block PR

### Auto-Generate Safety

- ✅ Draft PR only (not auto-merged)
- ✅ Require human review + approval before merge
- ✅ Limit to low-risk types (tests, config, docs; not core logic without flag)
- ✅ Sign all AI-generated commits with `[ai-generated]` metadata

### Pre-commit Checks

- ✅ Strict security checks (block known CVEs, unsafe patterns)
- ✅ Linting/type errors block push (fail-secure)
- ✅ Admin override available (for hotfixes, but logged)

## Future: Multi-Agent Orchestration

Once **Nexus Claw** (multi-agent framework) stabilizes:

```
Forge Webhook: PR created
  ↓
Orchestrator: Dispatch multi-agent task
  - @code-reviewer: Review code
  - @test-writer: Generate tests
  - @docs-writer: Update docs
  - @security-scanner: Check for vulns
  ↓
Consensus: All agents report
  ↓
Merge decision: All green? → Auto-approve
```

## Monitoring & Observability

### Agent Logs

```
[2024-01-01T12:00:00Z] task:code-review:pr-123 STARTED
[2024-01-01T12:00:05Z] task:code-review:pr-123 nexus-ai:/v1/chat/completions... CALLING
[2024-01-01T12:00:08Z] task:code-review:pr-123 ENSEMBLE: unanimous "approve" (3/3 providers)
[2024-01-01T12:00:09Z] task:code-review:pr-123 COMPLETED (8 suggestions)
```

### Metrics

- `agent_task_duration_ms` — Time to completion
- `agent_task_failures` — Failed tasks by type
- `nexus_ai_calls_total` — Total calls to AI service
- `ai_model_consensus_rate` — % of unanimous ensemble votes

---

**See**: [ARCHITECTURE.md](./ARCHITECTURE.md) for deep-dive on VCS backend design.
