# AnyClaw — Tool Configuration

## Built-in Tools

| Tool | Description |
|------|-------------|
| `shell` | Execute shell commands (with safety checks) |
| `read_file` | Read file contents |
| `write_file` | Write/create files |
| `list_dir` | List directory contents |
| `search_files` | Grep/search across files |
| `git` | Git operations |
| `http_request` | Make HTTP requests (no SSRF) |
| `memory_search` | Search episodic memory |
| `memory_store` | Store to vault |
| `vault_read` | Read vault notes |
| `memory_status` | Memory system status |

## GSD Tools (Auto-registered)

| Tool | Description |
|------|-------------|
| `gsd_create_spec` | Create a new GSD spec |
| `gsd_add_task` | Add a task to a spec |
| `gsd_next_task` | Get the next pending task |
| `gsd_complete_task` | Mark a task as done |
| `gsd_progress` | View progress on a spec |
| `gsd_list_specs` | List all specs |

## Adding Custom Tools

Tools follow the MCP-compatible interface. Register them via the tool registry:

```typescript
registry.register({
  name: "my_tool",
  description: "What it does",
  parameters: [
    { name: "input", type: "string", description: "The input", required: true }
  ],
  execute: async (args, context) => {
    return { result: "done" };
  }
});
```
