# AnyClaw — Agent Definitions

## Main Agent (default)

The primary agent for all general-purpose tasks.

- **Model**: Uses configured default model
- **Capabilities**: Full tool access, memory, GSD task management
- **Safety**: All guardrails active

## Patterns

When spawning sub-agents for specific tasks, use the orchestrator:

### Research Agent
Focus: Information gathering, code exploration, documentation reading.
Restrictions: Read-only tools (no write_file, no shell writes).

### Code Agent  
Focus: Code generation, refactoring, bug fixing.
Tools: Full filesystem + git access.

### Review Agent
Focus: Code review, security audit, best practices checking.
Restrictions: Read-only, produces reports.
