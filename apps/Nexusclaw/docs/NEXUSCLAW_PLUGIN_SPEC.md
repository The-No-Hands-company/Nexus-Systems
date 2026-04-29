# Nexusclaw Plugin Specification (Phase 1)

## Overview

Nexusclaw supports a lightweight plugin system that can extend the runtime with additional tools, LLM providers, skills, and event hooks. Plugins are loaded from local directories, single JS/TS files, or npm packages.

---

## Plugin Manifest

A plugin must declare a manifest. The manifest is read from `plugin.json` first, falling back to `package.json`.

### `plugin.json` (preferred)

```json
{
  "name": "my-plugin",
  "version": "1.0.0",
  "description": "What this plugin does",
  "main": "index.js",
  "provides": ["tools", "skill", "hooks"]
}
```

### Manifest Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | ✅ | Unique plugin identifier |
| `version` | `string` | ✅ | Semver version string |
| `description` | `string` | — | Human-readable description |
| `main` | `string` | — | Entry point relative to plugin dir (default: `index.js`) |
| `provides` | `string[]` | — | Capability hints: `"tools"`, `"provider"`, `"skill"`, `"hooks"` |

---

## Plugin Entry Point

The entry point (`main`) must be a valid ES module. It can export any combination of the following:

### `tools` — `Tool[]`

An array of tool definitions that will be registered in the tool registry.

```typescript
export const tools = [
  {
    name: "my_tool",
    description: "Does something useful",
    parameters: [
      { name: "input", type: "string", description: "Input value", required: true },
    ],
    async execute(args, context) {
      return { result: args.input };
    },
  },
];
```

Or a single tool via `tool`:

```typescript
export const tool = { name: "...", ... };
```

### `providers` — `{ prefix: string; provider: LLMProvider }[]`

An array of LLM provider registrations.

```typescript
export const providers = [
  { prefix: "myprovider/", provider: myLLMProvider },
];
```

Or a single provider via `provider`.

### `skills` — `Skill[]`

An array of skill definitions.

```typescript
export const skills = [
  {
    id: "my-skill",
    name: "My Skill",
    description: "Adds specialized knowledge",
    instructions: "You are an expert in...",
    enabled: true,
    source: "my-plugin",
  },
];
```

### `hooks` — `PluginHook[]`

An array of lifecycle hook handlers.

```typescript
export const hooks = [
  {
    event: "before_tool",
    handler: async (toolName, args) => {
      console.log(`About to call tool: ${toolName}`);
    },
  },
];
```

**Supported hook events:**
| Event | Trigger |
|---|---|
| `before_tool` | Before any tool execution |
| `after_tool` | After a tool execution |
| `before_llm` | Before an LLM API call |
| `after_llm` | After an LLM API call |
| `on_message` | When a new message is received |
| `on_error` | When an error occurs |

### `setup(context)` — Programmatic registration

For more control, export a `setup` function:

```typescript
export function setup(ctx) {
  ctx.registerTool({ name: "dynamic_tool", ... });
  ctx.registerSkill({ id: "my-skill", ... });
  ctx.registerHook({ event: "after_tool", handler: () => {} });
}
```

`setup` can be `async`. The context object exposes:
- `registerTool(tool: Tool)`
- `registerProvider(prefix: string, provider: LLMProvider)`
- `registerSkill(skill: Skill)`
- `registerHook(hook: PluginHook)`

---

## Loading Plugins

### Configuration (`anyclaw.yaml`)

```yaml
plugins:
  dirs:
    - ~/.anyclaw/plugins
    - ./plugins
  packages: []
```

Plugins in `dirs` subdirectories are auto-discovered. Each subdirectory must contain a `plugin.json` or `package.json`.

### CLI

```bash
# List all loaded plugins
anyclaw plugins list

# Load a specific plugin from a directory
anyclaw plugins load ./my-plugin

# Load from a single file
anyclaw plugins load ./tools/custom.js
```

### Gateway API

```http
GET /api/plugins
```

Returns:
```json
[
  {
    "name": "my-plugin",
    "version": "1.0.0",
    "description": "...",
    "tools": 2,
    "skills": 0
  }
]
```

---

## Example Plugin

A minimal plugin that adds a `greet` tool:

**`~/.anyclaw/plugins/greeting/plugin.json`**
```json
{
  "name": "greeting",
  "version": "1.0.0",
  "description": "Adds a greeting tool",
  "main": "index.js",
  "provides": ["tools"]
}
```

**`~/.anyclaw/plugins/greeting/index.js`**
```javascript
export const tools = [
  {
    name: "greet",
    description: "Returns a greeting for a name",
    parameters: [
      { name: "name", type: "string", description: "Name to greet", required: true }
    ],
    async execute(args) {
      return { message: `Hello, ${args.name}!` };
    },
  },
];
```

---

## Phase 1 Constraints

- Plugins run in the same Node.js process (no sandboxing in Phase 1).
- `async setup()` is best-effort; prefer sync setup for Phase 1 plugins.
- Plugin hot-reload is not supported in Phase 1; restart the runtime to pick up changes.
- npm package plugins are loaded via dynamic `import()` — the package must be installed in the project.
