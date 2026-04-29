// Hooks / Middleware system — pre/post hooks for tools, LLM, messages (Phase 10.3)
// Hooks can modify, log, cancel, or augment operations

import type { Tool, ToolContext } from "../core/types.js";

// ─── Hook Types ─────────────────────────────────────────────────────────

export type HookEvent =
  | "before_tool"
  | "after_tool"
  | "before_llm"
  | "after_llm"
  | "on_message"
  | "on_error";

export interface HookContext {
  /** The hook event type */
  event: HookEvent;
  /** Tool name (for tool hooks) */
  toolName?: string;
  /** Tool arguments (for before_tool — can be mutated) */
  toolArgs?: Record<string, unknown>;
  /** Tool result (for after_tool) */
  toolResult?: unknown;
  /** Tool execution context */
  toolContext?: ToolContext;
  /** LLM request messages (for before_llm) */
  llmMessages?: unknown[];
  /** LLM model (for llm hooks) */
  llmModel?: string;
  /** LLM response (for after_llm) */
  llmResponse?: unknown;
  /** User message content (for on_message) */
  messageContent?: string;
  /** Error object (for on_error) */
  error?: Error;
  /** Set to true in before_* hooks to cancel the operation */
  cancelled?: boolean;
  /** Reason for cancellation */
  cancelReason?: string;
  /** Arbitrary metadata hooks can attach */
  metadata: Record<string, unknown>;
}

export type HookHandler = (ctx: HookContext) => void | Promise<void>;

export interface RegisteredHook {
  id: string;
  event: HookEvent;
  handler: HookHandler;
  /** Higher priority runs first (default: 0) */
  priority: number;
  source: string;
}

// ─── Hook Runner ────────────────────────────────────────────────────────

export class HookRunner {
  private hooks = new Map<HookEvent, RegisteredHook[]>();
  private nextId = 1;

  /** Register a hook */
  register(
    event: HookEvent,
    handler: HookHandler,
    opts: { priority?: number; source?: string } = {},
  ): string {
    const id = `hook-${this.nextId++}`;
    const hook: RegisteredHook = {
      id,
      event,
      handler,
      priority: opts.priority ?? 0,
      source: opts.source ?? "unknown",
    };

    const list = this.hooks.get(event) || [];
    list.push(hook);
    // Sort by priority descending (highest first)
    list.sort((a, b) => b.priority - a.priority);
    this.hooks.set(event, list);

    return id;
  }

  /** Unregister a hook by ID */
  unregister(id: string): boolean {
    for (const [event, list] of this.hooks) {
      const idx = list.findIndex((h) => h.id === id);
      if (idx !== -1) {
        list.splice(idx, 1);
        return true;
      }
    }
    return false;
  }

  /** Unregister all hooks from a given source */
  unregisterSource(source: string): number {
    let count = 0;
    for (const [event, list] of this.hooks) {
      const before = list.length;
      this.hooks.set(
        event,
        list.filter((h) => h.source !== source),
      );
      count += before - (this.hooks.get(event)?.length || 0);
    }
    return count;
  }

  /** Run all hooks for an event, returning the (possibly mutated) context */
  async run(event: HookEvent, ctx: Partial<HookContext> = {}): Promise<HookContext> {
    const hookCtx: HookContext = {
      event,
      metadata: {},
      ...ctx,
    };

    const hooks = this.hooks.get(event) || [];
    for (const hook of hooks) {
      try {
        await hook.handler(hookCtx);
      } catch (err) {
        // Hook errors shouldn't crash the system; log and continue
        console.warn(`[Hook] Error in ${hook.source}/${hook.id}: ${(err as Error).message}`);
      }

      // If a before_* hook cancelled, stop running remaining hooks
      if (hookCtx.cancelled && event.startsWith("before_")) {
        break;
      }
    }

    return hookCtx;
  }

  /** Get count of registered hooks per event */
  stats(): Record<HookEvent, number> {
    const events: HookEvent[] = ["before_tool", "after_tool", "before_llm", "after_llm", "on_message", "on_error"];
    const result: Record<string, number> = {};
    for (const e of events) {
      result[e] = this.hooks.get(e)?.length || 0;
    }
    return result as Record<HookEvent, number>;
  }

  /** List all registered hooks */
  list(): RegisteredHook[] {
    return [...this.hooks.values()].flat();
  }
}
