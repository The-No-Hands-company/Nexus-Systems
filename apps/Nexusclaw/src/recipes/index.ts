// Recipe system — pre-built workflow templates (Phase 13.6)
// Recipes are reusable multi-step workflows the agent can execute

import type { Tool, ToolContext } from "../core/types.js";

// ─── Recipe Types ───────────────────────────────────────────────────────

export interface RecipeStep {
  /** Human-readable step description */
  description: string;
  /** Tool to invoke (or "llm" for a direct LLM call) */
  tool: string;
  /** Arguments for the tool (can reference {{variables}} from prior steps) */
  args: Record<string, unknown>;
  /** Key to store step result under for later reference */
  outputKey?: string;
  /** If true, failure of this step doesn't abort the recipe */
  optional?: boolean;
}

export interface Recipe {
  id: string;
  name: string;
  description: string;
  /** Tags for discoverability */
  tags: string[];
  /** Input variables the recipe needs */
  inputs: { name: string; description: string; required: boolean; default?: string }[];
  /** Ordered steps */
  steps: RecipeStep[];
}

export interface RecipeResult {
  recipeId: string;
  success: boolean;
  outputs: Record<string, unknown>;
  stepResults: { step: string; success: boolean; result?: unknown; error?: string }[];
}

// ─── Built-in Recipes ───────────────────────────────────────────────────

const codeReviewRecipe: Recipe = {
  id: "code-review",
  name: "Code Review",
  description: "Comprehensive code review — reads files, identifies issues, and suggests improvements.",
  tags: ["code", "review", "quality"],
  inputs: [
    { name: "path", description: "File or directory to review", required: true },
    { name: "focus", description: "Focus areas (e.g., security, performance, readability)", required: false, default: "all" },
  ],
  steps: [
    { description: "Read the target file(s)", tool: "read_file", args: { path: "{{path}}" }, outputKey: "source" },
    { description: "Search for TODO/FIXME/HACK markers", tool: "search_files", args: { pattern: "TODO|FIXME|HACK|XXX", path: "{{path}}" }, outputKey: "markers", optional: true },
  ],
};

const bugHuntRecipe: Recipe = {
  id: "bug-hunt",
  name: "Bug Hunt",
  description: "Systematic bug investigation — searches for error patterns, traces execution, reviews related code.",
  tags: ["debug", "bug", "investigation"],
  inputs: [
    { name: "description", description: "Description of the bug or error message", required: true },
    { name: "path", description: "Directory to search in", required: false, default: "." },
  ],
  steps: [
    { description: "Search for error strings in codebase", tool: "search_files", args: { pattern: "{{description}}", path: "{{path}}" }, outputKey: "errorMatches" },
    { description: "Search for related try/catch blocks", tool: "search_files", args: { pattern: "catch|throw|Error", path: "{{path}}" }, outputKey: "errorHandlers", optional: true },
    { description: "Check recent git changes", tool: "git", args: { subcommand: "log --oneline -20" }, outputKey: "recentCommits", optional: true },
  ],
};

const docGenRecipe: Recipe = {
  id: "doc-gen",
  name: "Documentation Generator",
  description: "Generate documentation for a codebase — reads source files and produces structured docs.",
  tags: ["docs", "documentation", "generate"],
  inputs: [
    { name: "path", description: "Source file or directory to document", required: true },
    { name: "format", description: "Output format (markdown, jsdoc)", required: false, default: "markdown" },
  ],
  steps: [
    { description: "List source files", tool: "list_dir", args: { path: "{{path}}" }, outputKey: "files" },
    { description: "Read the source", tool: "read_file", args: { path: "{{path}}" }, outputKey: "source" },
  ],
};

const projectScanRecipe: Recipe = {
  id: "project-scan",
  name: "Project Scanner",
  description: "Scan a project to understand its structure, dependencies, and architecture.",
  tags: ["project", "analysis", "overview"],
  inputs: [
    { name: "path", description: "Project root directory", required: false, default: "." },
  ],
  steps: [
    { description: "List project root", tool: "list_dir", args: { path: "{{path}}" }, outputKey: "rootFiles" },
    { description: "Check package.json", tool: "read_file", args: { path: "{{path}}/package.json" }, outputKey: "packageJson", optional: true },
    { description: "Check for README", tool: "read_file", args: { path: "{{path}}/README.md" }, outputKey: "readme", optional: true },
    { description: "Git status", tool: "git", args: { subcommand: "status --short" }, outputKey: "gitStatus", optional: true },
    { description: "Recent git log", tool: "git", args: { subcommand: "log --oneline -10" }, outputKey: "gitLog", optional: true },
  ],
};

const securityAuditRecipe: Recipe = {
  id: "security-audit",
  name: "Security Audit",
  description: "Basic security audit — checks for common vulnerabilities, secrets in code, and dependency issues.",
  tags: ["security", "audit", "vulnerabilities"],
  inputs: [
    { name: "path", description: "Directory to audit", required: false, default: "." },
  ],
  steps: [
    { description: "Search for hardcoded secrets", tool: "search_files", args: { pattern: "password|secret|api_key|apikey|token.*=\\s*['\"]", path: "{{path}}" }, outputKey: "secrets" },
    { description: "Search for eval/exec usage", tool: "search_files", args: { pattern: "eval\\(|exec\\(|execSync\\(|Function\\(", path: "{{path}}" }, outputKey: "dangerousCalls" },
    { description: "Search for SQL injection vectors", tool: "search_files", args: { pattern: "\\$\\{.*\\}.*(?:SELECT|INSERT|UPDATE|DELETE|DROP)", path: "{{path}}" }, outputKey: "sqlInjection", optional: true },
    { description: "Check npm audit", tool: "shell", args: { command: "npm audit --json 2>/dev/null | head -100", timeout: 15000 }, outputKey: "npmAudit", optional: true },
  ],
};

// ─── Recipe Registry ────────────────────────────────────────────────────

export class RecipeRegistry {
  private recipes = new Map<string, Recipe>();

  constructor() {
    // Register built-in recipes
    for (const recipe of [codeReviewRecipe, bugHuntRecipe, docGenRecipe, projectScanRecipe, securityAuditRecipe]) {
      this.recipes.set(recipe.id, recipe);
    }
  }

  register(recipe: Recipe): void {
    this.recipes.set(recipe.id, recipe);
  }

  get(id: string): Recipe | undefined {
    return this.recipes.get(id);
  }

  list(): Recipe[] {
    return [...this.recipes.values()];
  }

  /** Execute a recipe by running each step through the tool registry */
  async execute(
    recipeId: string,
    inputs: Record<string, string>,
    toolExecutor: (toolName: string, args: Record<string, unknown>, context: ToolContext) => Promise<unknown>,
    context: ToolContext,
  ): Promise<RecipeResult> {
    const recipe = this.recipes.get(recipeId);
    if (!recipe) throw new Error(`Recipe not found: ${recipeId}`);

    // Validate required inputs
    for (const input of recipe.inputs) {
      if (input.required && !inputs[input.name]) {
        throw new Error(`Missing required input: ${input.name} — ${input.description}`);
      }
    }

    // Fill defaults
    const vars: Record<string, unknown> = { ...inputs };
    for (const input of recipe.inputs) {
      if (!vars[input.name] && input.default !== undefined) {
        vars[input.name] = input.default;
      }
    }

    const stepResults: RecipeResult["stepResults"] = [];
    let success = true;

    for (const step of recipe.steps) {
      // Resolve template variables in args
      const resolvedArgs: Record<string, unknown> = {};
      for (const [key, val] of Object.entries(step.args)) {
        if (typeof val === "string" && val.includes("{{")) {
          resolvedArgs[key] = val.replace(/\{\{(\w+)\}\}/g, (_, varName) => String(vars[varName] ?? ""));
        } else {
          resolvedArgs[key] = val;
        }
      }

      try {
        const result = await toolExecutor(step.tool, resolvedArgs, context);
        if (step.outputKey) vars[step.outputKey] = result;
        stepResults.push({ step: step.description, success: true, result });
      } catch (err: any) {
        stepResults.push({ step: step.description, success: false, error: err.message });
        if (!step.optional) {
          success = false;
          break;
        }
      }
    }

    return {
      recipeId,
      success,
      outputs: vars,
      stepResults,
    };
  }
}

// ─── Recipe Tools ───────────────────────────────────────────────────────

export function createRecipeTools(registry: RecipeRegistry): Tool[] {
  return [
    {
      name: "recipe_list",
      description: "List all available recipes (pre-built workflow templates)",
      parameters: [],
      async execute() {
        return registry.list().map((r) => ({
          id: r.id,
          name: r.name,
          description: r.description,
          tags: r.tags,
          inputs: r.inputs,
        }));
      },
    },
    {
      name: "recipe_run",
      description: "Run a recipe (pre-built workflow). Use recipe_list to see available recipes.",
      parameters: [
        { name: "recipe_id", type: "string", description: "Recipe ID to run", required: true },
        { name: "inputs", type: "object", description: "Input variables for the recipe", required: false },
      ],
      async execute(args, context) {
        const recipeId = args.recipe_id as string;
        const inputs = (args.inputs as Record<string, string>) || {};

        // The recipe executor calls tools through the same context
        const result = await registry.execute(
          recipeId,
          inputs,
          async (toolName, toolArgs, ctx) => {
            // This will be replaced during wiring to use the actual ToolRegistry
            throw new Error(`Tool execution not wired: ${toolName}`);
          },
          context,
        );

        return result;
      },
    },
  ];
}
