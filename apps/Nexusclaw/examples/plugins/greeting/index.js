/**
 * greeting — sample AnyClaw plugin
 *
 * Demonstrates the Phase 1 plugin contract:
 *   - Exports `tools` (Tool[])
 *   - Exports `hooks` (PluginHook[])
 *
 * Load with:
 *   node dist/index.js plugins load examples/plugins/greeting
 */

// ─── Tools ────────────────────────────────────────────────────────────────

export const tools = [
  {
    name: "greet",
    description: "Returns a personalised greeting for the given name.",
    parameters: [
      { name: "name", type: "string", description: "Name of the person to greet", required: true },
      { name: "style", type: "string", description: "Greeting style: formal | casual (default: casual)", required: false, default: "casual" },
    ],
    async execute(args) {
      const name = (args.name || "stranger").toString().trim();
      const style = (args.style || "casual").toString().toLowerCase();
      const greeting = style === "formal"
        ? `Good day, ${name}. It is a pleasure to meet you.`
        : `Hey ${name}! 👋 Great to see you!`;
      return { greeting, name, style };
    },
  },
  {
    name: "farewell",
    description: "Returns a farewell message for the given name.",
    parameters: [
      { name: "name", type: "string", description: "Name of the person to say goodbye to", required: true },
    ],
    async execute(args) {
      const name = (args.name || "friend").toString().trim();
      return { farewell: `Goodbye, ${name}! Until next time. 👋`, name };
    },
  },
];

// ─── Hooks ────────────────────────────────────────────────────────────────

export const hooks = [
  {
    event: "before_tool",
    handler(toolName, args) {
      // Example: log tool calls that use this plugin's tools
      if (toolName === "greet" || toolName === "farewell") {
        console.debug(`[greeting-plugin] ${toolName} called`, args);
      }
    },
  },
];
