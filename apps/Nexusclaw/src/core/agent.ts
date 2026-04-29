// Core agent loop — the observe → plan → act → reflect cycle
// Phase 1.1: Persistent sessions via MemoryManager
// Phase 1.5: Streaming responses
// Phase 1.7: Extended thinking pass-through
// Phase 5: Skills injection
// Phase 7.1-7.5: Advanced agent config (tool policy, sandbox, persona)
// Phase 13.7: Context file loading

import { randomUUID } from "node:crypto";
import type {
  AgentConfig,
  AgentState,
  Message,
  Session,
  LLMMessage,
  LLMContentBlock,
  Permissions,
  StreamEvent,
  Skill,
} from "./types.js";
import type { LLMRouter } from "../llm/index.js";
import type { ToolRegistry } from "../tools/index.js";
import type { MemoryManager } from "../memory/index.js";
import type { GSDManager } from "../gsd/index.js";
import type { SafetyManager } from "../safety/index.js";
import type { HookRunner } from "../hooks/index.js";
import { loadContextFiles } from "../config/index.js";
import { metrics } from "../metrics/index.js";
import { webhooks } from "../webhooks/index.js";

export interface AgentDeps {
  llm: LLMRouter;
  tools: ToolRegistry;
  memory: MemoryManager;
  gsd: GSDManager;
  safety: SafetyManager;
  config: AgentConfig;
  hooks?: HookRunner;
}

export class Agent {
  readonly id: string;
  readonly config: AgentConfig;
  private llm: LLMRouter;
  private tools: ToolRegistry;
  private memory: MemoryManager;
  private gsd: GSDManager;
  private safety: SafetyManager;
  private hooks?: HookRunner;
  private skills: Skill[] = [];
  private state: AgentState;

  constructor(deps: AgentDeps) {
    this.id = deps.config.id;
    this.config = deps.config;
    this.llm = deps.llm;
    this.tools = deps.tools;
    this.memory = deps.memory;
    this.gsd = deps.gsd;
    this.safety = deps.safety;
    this.hooks = deps.hooks;

    this.state = {
      phase: "observe",
      sessionId: "",
      turnCount: 0,
      pendingToolCalls: [],
      reflections: [],
    };
  }

  // ─── Skills (Phase 5) ─────────────────────────────────────────────

  loadSkills(skills: Skill[]): void {
    this.skills = skills.filter((s) => {
      if (!s.enabled) return false;
      // Phase 5.4: If agents list is set and non-empty, only load for matching agents
      if (s.agents && s.agents.length > 0) {
        return s.agents.includes(this.id);
      }
      return true; // shared with all agents
    });
    // Register skill-provided tools
    for (const skill of this.skills) {
      if (skill.tools) {
        for (const tool of skill.tools) {
          this.tools.register(tool);
        }
      }
    }
  }

  // ─── Agent-Level Tool Policy (Phase 7.1) ──────────────────────────

  /** Check if a tool is permitted by this agent's policy */
  private isToolPermitted(toolName: string): boolean {
    const { allowedTools, deniedTools } = this.config;
    if (deniedTools && deniedTools.includes(toolName)) return false;
    if (allowedTools && allowedTools.length > 0) return allowedTools.includes(toolName);
    return true;
  }

  /** Get tools filtered by agent-level allow/deny lists */
  private getPermittedTools() {
    return this.tools.list().filter(t => this.isToolPermitted(t.name));
  }

  /** Get LLM tool definitions filtered by agent-level policy */
  private getPermittedLLMTools() {
    return this.tools.toLLMTools().filter(t => this.isToolPermitted(t.name));
  }

  // ─── Session Management (Phase 1.1: persistent) ──────────────────

  createSession(channel?: string): Session {
    return this.memory.createSession(this.id, channel);
  }

  getSession(sessionId: string): Session | undefined {
    return this.memory.getSession(sessionId) ?? undefined;
  }

  // ─── System Prompt Construction ────────────────────────────────────

  private buildSystemPrompt(): string {
    let prompt = "";

    // Soul / Identity / Persona (Phase 7.5)
    if (this.config.soul) {
      prompt += `${this.config.soul}\n\n`;
    } else if (this.config.persona) {
      if (typeof this.config.persona === "string") {
        prompt += `You are ${this.config.persona}.\n\n`;
      } else {
        const p = this.config.persona;
        prompt += `You are ${p.name || this.config.name}`;
        if (p.role) prompt += `, ${p.role}`;
        prompt += ".\n\n";
        if (p.personality && p.personality.length > 0) {
          prompt += `## Personality\n${p.personality.map(t => `- ${t}`).join("\n")}\n\n`;
        }
        if (p.voice) {
          prompt += `## Voice & Tone\n${p.voice}\n\n`;
        }
        if (p.expertise && p.expertise.length > 0) {
          prompt += `## Expertise\n${p.expertise.map(e => `- ${e}`).join("\n")}\n\n`;
        }
        if (p.constraints && p.constraints.length > 0) {
          prompt += `## Constraints\n${p.constraints.map(c => `- ${c}`).join("\n")}\n\n`;
        }
      }
    } else {
      prompt += `You are AnyClaw, a powerful personal AI development assistant. You help with coding, automation, research, and project management. You are thorough, safe, and reliable.\n\n`;
    }

    // Vault context (self-knowledge from memory)
    const vaultContext = this.memory.getVaultContext();
    if (vaultContext) {
      prompt += `${vaultContext}\n`;
    }

    // Context files (Phase 13.7)
    if (this.config.contextFiles && this.config.contextFiles.length > 0) {
      const ctx = loadContextFiles(this.config.contextFiles, process.cwd());
      if (ctx) {
        prompt += `## Project Context\n${ctx}\n\n`;
      }
    } else {
      // Auto-discover .context.md
      const ctx = loadContextFiles([], process.cwd());
      if (ctx) {
        prompt += `## Project Context\n${ctx}\n\n`;
      }
    }

    // Skills instructions (Phase 5)
    if (this.skills.length > 0) {
      prompt += `## Active Skills\n`;
      for (const skill of this.skills) {
        prompt += `### ${skill.name}\n${skill.instructions}\n\n`;
      }
    }

    // Available tools (filtered by agent-level policy — Phase 7.1)
    const tools = this.getPermittedTools();
    if (tools.length > 0) {
      prompt += `## Available Tools\n`;
      prompt += `You have ${tools.length} tools available. Use them to accomplish tasks:\n`;
      for (const tool of tools) {
        prompt += `- **${tool.name}**: ${tool.description}\n`;
      }
      prompt += "\n";
    }

    // Safety rules
    if (!this.safety.isElevated()) {
      prompt += `## Safety Rules\n`;
      prompt += `- NEVER execute commands that could damage the system\n`;
      prompt += `- ALWAYS verify file paths are within allowed directories\n`;
      prompt += `- ASK for confirmation before destructive operations\n`;
      prompt += `- If unsure, explain what you want to do and ask for approval\n\n`;
    }

    // GSD rules
    prompt += `## Task Execution Rules (GSD)\n`;
    prompt += `- Break large tasks into atomic, verifiable sub-tasks\n`;
    prompt += `- Focus on ONE task at a time\n`;
    prompt += `- Each task should have clear DONE criteria\n`;
    prompt += `- Verify ALL criteria before marking a task complete\n`;

    return prompt;
  }

  // ─── Core Agent Loop ──────────────────────────────────────────────

  async processMessage(
    userMessage: string,
    sessionId?: string,
  ): Promise<AgentResponse> {
    // Get or create session (persistent via MemoryManager)
    let session: Session;
    if (sessionId) {
      const existing = this.memory.getSession(sessionId);
      if (!existing) throw new Error(`Session not found: ${sessionId}`);
      session = existing;
    } else {
      session = this.createSession();
    }

    // Store user message
    const userMsg: Message = {
      id: randomUUID(),
      role: "user",
      content: userMessage,
      timestamp: Date.now(),
    };
    session.messages.push(userMsg);
    this.memory.pushMessage(session.id, userMsg);

    // Hook: on_message
    if (this.hooks) {
      await this.hooks.run("on_message", { messageContent: userMessage });
    }

    // Phase 6: emit session.created for new sessions and chat.message for user turn
    const isNewSession = !sessionId;
    if (isNewSession) webhooks.emit("session.created", { agentId: this.id, sessionId: session.id });
    webhooks.emit("chat.message", { agentId: this.id, sessionId: session.id, role: "user", content: userMessage });

    // Build LLM request
    const systemPrompt = this.buildSystemPrompt();
    const llmMessages: LLMMessage[] = [
      { role: "system", content: systemPrompt },
    ];

    // Add conversation history
    const sessionMessages = this.memory.getSessionMessages(session.id);
    for (const msg of sessionMessages) {
      if (msg.role === "system") continue;
      llmMessages.push({
        role: msg.role as "user" | "assistant",
        content: msg.content,
      });
    }

    // Tool loop
    let maxIterations = this.config.maxToolIterations ?? 20;
    const allToolResults: { name: string; result: unknown }[] = [];
    let finalResponse = "";
    let thinkingContent = "";
    let totalInputTokens = 0;
    let totalOutputTokens = 0;

    while (maxIterations-- > 0) {
      this.state.phase = "plan";

      // Hook: before_llm
      if (this.hooks) {
        const hookCtx = await this.hooks.run("before_llm", {
          llmMessages,
          llmModel: this.config.model,
        });
        if (hookCtx.cancelled) {
          finalResponse = hookCtx.cancelReason || "LLM call cancelled by hook";
          break;
        }
      }

      const _llmStart = Date.now();
      const llmResponse = await this.llm.chatWithFallback(
        {
          model: this.config.model,
          messages: llmMessages,
          tools: this.getPermittedLLMTools(),
          maxTokens: this.config.maxTokens || 8192,
          temperature: this.config.temperature,
          thinking: this.config.thinking,
        },
        this.config.fallbackModel,
      );
      // Phase 6: record LLM latency + token usage
      metrics.recordLatency({ agentId: this.id, type: "llm", durationMs: Date.now() - _llmStart });
      metrics.recordTokenUsage({
        agentId: this.id, sessionId: session.id,
        model: this.config.model,
        inputTokens: llmResponse.usage.inputTokens,
        outputTokens: llmResponse.usage.outputTokens,
      });

      // Hook: after_llm
      if (this.hooks) {
        await this.hooks.run("after_llm", {
          llmResponse,
          llmModel: this.config.model,
        });
      }

      totalInputTokens += llmResponse.usage.inputTokens;
      totalOutputTokens += llmResponse.usage.outputTokens;

      // Extract all block types
      const textBlocks = llmResponse.content.filter((b) => b.type === "text" && b.text);
      const toolCallBlocks = llmResponse.content.filter((b) => b.type === "tool_use");
      const thinkingBlocks = llmResponse.content.filter((b) => b.type === "thinking");

      // Collect thinking (Phase 1.7)
      if (thinkingBlocks.length > 0) {
        thinkingContent += thinkingBlocks.map((b) => b.text).join("\n");
      }

      // Collect text
      const responseText = textBlocks.map((b) => b.text!).join("\n");
      if (responseText) finalResponse = responseText;

      // No tool calls → done
      if (toolCallBlocks.length === 0) break;

      // Execute tool calls
      this.state.phase = "act";

      llmMessages.push({
        role: "assistant",
        content: llmResponse.content,
      });

      const toolResultBlocks: LLMContentBlock[] = [];

      // Phase 7.7: Parallel tool execution — batch independent tool calls
      // First pass: safety checks + confirmations (must be sequential for interactive prompts)
      const approved: { block: LLMContentBlock; name: string; args: Record<string, unknown>; id: string }[] = [];
      for (const block of toolCallBlocks) {
        const toolName = block.name!;
        const toolArgs = (block.input as Record<string, unknown>) || {};
        const toolCallId = block.id!;

        // Agent-level tool policy (Phase 7.1)
        if (!this.isToolPermitted(toolName)) {
          const errorResult = `BLOCKED: Tool "${toolName}" is not permitted for agent "${this.config.name}"`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: errorResult });
          this.safety.logAction({ agentId: this.id, toolName, args: toolArgs, error: errorResult });
          webhooks.emit("safety.blocked", { agentId: this.id, sessionId: session.id, toolName, reason: errorResult });
          metrics.recordToolCall({ agentId: this.id, toolName, durationMs: 0, success: false, error: errorResult });
          continue;
        }

        const verdict = this.safety.checkAction({ agentId: this.id, toolName, args: toolArgs });

        if (!verdict.allowed) {
          const errorResult = `BLOCKED: ${verdict.reason}`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: errorResult });
          this.safety.logAction({ agentId: this.id, toolName, args: toolArgs, error: errorResult });
          webhooks.emit("safety.blocked", { agentId: this.id, sessionId: session.id, toolName, reason: verdict.reason ?? errorResult });
          metrics.recordToolCall({ agentId: this.id, toolName, durationMs: 0, success: false, error: errorResult });
          continue;
        }

        if (verdict.requiresConfirmation) {
          const confirmed = await this.safety.requestInteractiveConfirmation(
            toolName,
            verdict.confirmationDetails || `Execute ${toolName}?`,
          );
          if (!confirmed) {
            toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: "BLOCKED: User denied confirmation" });
            continue;
          }
        }

        for (const warning of verdict.warnings) {
          console.warn(`[Safety] ${warning}`);
        }

        approved.push({ block, name: toolName, args: toolArgs, id: toolCallId });
      }

      // Second pass: execute approved tools in parallel
      const toolContext = {
        agentId: this.id,
        sessionId: session.id,
        workingDir: this.config.sandboxDir || process.cwd(),
        sandboxRoot: this.config.sandboxRoot,
        permissions: this.safety.getPermissions(),
      };

      // Mutating tools (shell, write_file, git, apply_patch, process_spawn) run sequentially
      // Read-only tools can run in parallel
      const mutatingTools = new Set([
        "shell", "write_file", "git", "apply_patch", "process_spawn", "process_kill",
        "browser_click", "browser_type", "browser_navigate", "browser_eval", "browser_close",
      ]);
      const sequential = approved.filter((t) => mutatingTools.has(t.name));
      const parallel = approved.filter((t) => !mutatingTools.has(t.name));

      // Run read-only tools in parallel
      if (parallel.length > 0) {
        const results = await Promise.allSettled(
          parallel.map(async (t) => {
            // Hook: before_tool
            if (this.hooks) {
              const hookCtx = await this.hooks.run("before_tool", {
                toolName: t.name, toolArgs: t.args, toolContext,
              });
              if (hookCtx.cancelled) return { ...t, result: `CANCELLED: ${hookCtx.cancelReason || "by hook"}` };
            }
            const _t0 = Date.now();
            const result = await this.tools.execute(t.name, t.args, toolContext);
            const durationMs = Date.now() - _t0;
            // Hook: after_tool
            if (this.hooks) {
              await this.hooks.run("after_tool", {
                toolName: t.name, toolArgs: t.args, toolResult: result, toolContext,
              });
            }
            return { ...t, result, durationMs };
          }),
        );
        for (let i = 0; i < results.length; i++) {
          const t = parallel[i];
          const r = results[i];
          if (r.status === "fulfilled") {
            const resultStr = typeof r.value.result === "string" ? r.value.result : JSON.stringify(r.value.result, null, 2);
            toolResultBlocks.push({ type: "tool_result", toolUseId: t.id, content: resultStr.slice(0, 50000) });
            allToolResults.push({ name: t.name, result: r.value.result });
            this.safety.logAction({ agentId: this.id, toolName: t.name, args: t.args, result: String(r.value.result).slice(0, 200) });
            metrics.recordToolCall({ agentId: this.id, toolName: t.name, durationMs: r.value.durationMs ?? 0, success: true });
            webhooks.emit("tool.called", { agentId: this.id, sessionId: session.id, toolName: t.name, result: resultStr.slice(0, 500) });
          } else {
            const errorStr = `Error: ${r.reason?.message || r.reason}`;
            toolResultBlocks.push({ type: "tool_result", toolUseId: t.id, content: errorStr });
            this.safety.logAction({ agentId: this.id, toolName: t.name, args: t.args, error: errorStr });
            metrics.recordToolCall({ agentId: this.id, toolName: t.name, durationMs: 0, success: false, error: errorStr });
            webhooks.emit("tool.failed", { agentId: this.id, sessionId: session.id, toolName: t.name, error: errorStr });
          }
        }
      }

      // Run mutating tools sequentially
      for (const t of sequential) {
        try {
          // Hook: before_tool
          if (this.hooks) {
            const hookCtx = await this.hooks.run("before_tool", {
              toolName: t.name, toolArgs: t.args, toolContext,
            });
            if (hookCtx.cancelled) {
              toolResultBlocks.push({ type: "tool_result", toolUseId: t.id, content: `CANCELLED: ${hookCtx.cancelReason || "by hook"}` });
              continue;
            }
          }
          const _t0 = Date.now();
          const result = await this.tools.execute(t.name, t.args, toolContext);
          const _dur = Date.now() - _t0;
          // Hook: after_tool
          if (this.hooks) {
            await this.hooks.run("after_tool", {
              toolName: t.name, toolArgs: t.args, toolResult: result, toolContext,
            });
          }
          const resultStr = typeof result === "string" ? result : JSON.stringify(result, null, 2);
          toolResultBlocks.push({ type: "tool_result", toolUseId: t.id, content: resultStr.slice(0, 50000) });
          allToolResults.push({ name: t.name, result });
          this.safety.logAction({ agentId: this.id, toolName: t.name, args: t.args, result: resultStr.slice(0, 200) });
          metrics.recordToolCall({ agentId: this.id, toolName: t.name, durationMs: _dur, success: true });
          webhooks.emit("tool.called", { agentId: this.id, sessionId: session.id, toolName: t.name, result: resultStr.slice(0, 500) });
        } catch (err: any) {
          const errorStr = `Error: ${err.message}`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: t.id, content: errorStr });
          this.safety.logAction({ agentId: this.id, toolName: t.name, args: t.args, error: errorStr });
          metrics.recordToolCall({ agentId: this.id, toolName: t.name, durationMs: 0, success: false, error: errorStr });
          webhooks.emit("tool.failed", { agentId: this.id, sessionId: session.id, toolName: t.name, error: errorStr });
        }
      }

      llmMessages.push({ role: "user", content: toolResultBlocks });
    }

    // Store assistant response
    this.state.phase = "reflect";
    const assistantMsg: Message = {
      id: randomUUID(),
      role: "assistant",
      content: finalResponse,
      timestamp: Date.now(),
    };
    session.messages.push(assistantMsg);
    session.updatedAt = Date.now();
    this.memory.pushMessage(session.id, assistantMsg);
    // Phase 6: emit chat.message for assistant response
    webhooks.emit("chat.message", { agentId: this.id, sessionId: session.id, role: "assistant", content: finalResponse });
    this.state.turnCount++;

    // Auto-flush to vault (Phase 15.4) — every N turns
    if (this.state.turnCount % 10 === 0) {
      this.autoFlushFromSession(session.id);
    }

    return {
      sessionId: session.id,
      content: finalResponse,
      thinking: thinkingContent || undefined,
      toolResults: allToolResults,
      usage: { inputTokens: totalInputTokens, outputTokens: totalOutputTokens },
    };
  }

  // ─── Streaming (Phase 1.5) ────────────────────────────────────────

  async *processMessageStream(
    userMessage: string,
    sessionId?: string,
  ): AsyncIterable<StreamEvent> {
    // Get or create session
    let session: Session;
    if (sessionId) {
      const existing = this.memory.getSession(sessionId);
      if (!existing) throw new Error(`Session not found: ${sessionId}`);
      session = existing;
    } else {
      session = this.createSession();
    }

    // Store user message
    const userMsg: Message = {
      id: randomUUID(),
      role: "user",
      content: userMessage,
      timestamp: Date.now(),
    };
    session.messages.push(userMsg);
    this.memory.pushMessage(session.id, userMsg);

    // Build LLM request
    const systemPrompt = this.buildSystemPrompt();
    const llmMessages: LLMMessage[] = [
      { role: "system", content: systemPrompt },
    ];
    const sessionMessages = this.memory.getSessionMessages(session.id);
    for (const msg of sessionMessages) {
      if (msg.role === "system") continue;
      llmMessages.push({ role: msg.role as "user" | "assistant", content: msg.content });
    }

    // Streaming tool loop
    let maxIterations = this.config.maxToolIterations ?? 20;
    const allToolResults: { name: string; result: unknown }[] = [];
    let finalResponse = "";
    let totalInputTokens = 0;
    let totalOutputTokens = 0;

    while (maxIterations-- > 0) {
      this.state.phase = "plan";

      // Use real streaming from LLM provider
      const streamEvents: StreamEvent[] = [];
      const contentBlocks: LLMContentBlock[] = [];
      let currentText = "";
      let pendingToolCalls: { id: string; name: string; input: string }[] = [];
      let currentToolJson = "";
      let currentToolId = "";
      let currentToolName = "";
      let hasToolCalls = false;

      for await (const event of this.llm.streamChat({
        model: this.config.model,
        messages: llmMessages,
        tools: this.getPermittedLLMTools(),
        maxTokens: this.config.maxTokens || 8192,
        temperature: this.config.temperature,
        thinking: this.config.thinking,
      })) {
        if (event.type === "text_delta") {
          currentText += event.content || "";
          yield event;
        } else if (event.type === "thinking") {
          yield event;
        } else if (event.type === "tool_start") {
          hasToolCalls = true;
          if (currentToolId) {
            pendingToolCalls.push({ id: currentToolId, name: currentToolName, input: currentToolJson });
          }
          currentToolId = event.content || "";
          currentToolName = event.toolName || "";
          currentToolJson = "";
          yield event;
        } else if (event.type === "done") {
          if (event.usage) {
            totalInputTokens += event.usage.inputTokens;
            totalOutputTokens += event.usage.outputTokens;
          }
        }
      }

      // Finalize last pending tool call
      if (currentToolId) {
        pendingToolCalls.push({ id: currentToolId, name: currentToolName, input: currentToolJson });
      }

      if (currentText) {
        contentBlocks.push({ type: "text", text: currentText });
        finalResponse = currentText;
      }

      // If no tool calls were streamed, try falling back to non-streaming for tool detection
      if (!hasToolCalls) break;

      // For tool execution, fall back to non-streaming to get proper tool_use blocks
      const llmResponse = await this.llm.chatWithFallback(
        {
          model: this.config.model,
          messages: llmMessages,
          tools: this.getPermittedLLMTools(),
          maxTokens: this.config.maxTokens || 8192,
          temperature: this.config.temperature,
          thinking: this.config.thinking,
        },
        this.config.fallbackModel,
      );

      totalInputTokens += llmResponse.usage.inputTokens;
      totalOutputTokens += llmResponse.usage.outputTokens;

      const toolCallBlocks = llmResponse.content.filter((b) => b.type === "tool_use");
      const textBlocks = llmResponse.content.filter((b) => b.type === "text" && b.text);
      const responseText = textBlocks.map((b) => b.text!).join("\n");
      if (responseText) finalResponse = responseText;

      if (toolCallBlocks.length === 0) break;

      // Execute tool calls
      this.state.phase = "act";
      llmMessages.push({ role: "assistant", content: llmResponse.content });
      const toolResultBlocks: LLMContentBlock[] = [];

      for (const block of toolCallBlocks) {
        const toolName = block.name!;
        const toolArgs = (block.input as Record<string, unknown>) || {};
        const toolCallId = block.id!;

        yield { type: "tool_start", toolName, content: toolCallId };

        // Agent-level tool policy (Phase 7.1)
        if (!this.isToolPermitted(toolName)) {
          const errorResult = `BLOCKED: Tool "${toolName}" is not permitted for agent "${this.config.name}"`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: errorResult });
          yield { type: "tool_result", toolName, toolResult: errorResult };
          continue;
        }

        const verdict = this.safety.checkAction({ agentId: this.id, toolName, args: toolArgs });

        if (!verdict.allowed) {
          const errorResult = `BLOCKED: ${verdict.reason}`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: errorResult });
          yield { type: "tool_result", toolName, toolResult: errorResult };
          continue;
        }

        if (verdict.requiresConfirmation) {
          const confirmed = await this.safety.requestInteractiveConfirmation(
            toolName, verdict.confirmationDetails || `Execute ${toolName}?`,
          );
          if (!confirmed) {
            toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: "BLOCKED: User denied" });
            yield { type: "tool_result", toolName, toolResult: "BLOCKED: User denied" };
            continue;
          }
        }

        try {
          const result = await this.tools.execute(toolName, toolArgs, {
            agentId: this.id, sessionId: session.id,
            workingDir: this.config.sandboxDir || process.cwd(),
            sandboxRoot: this.config.sandboxRoot,
            permissions: this.safety.getPermissions(),
          });
          const resultStr = typeof result === "string" ? result : JSON.stringify(result, null, 2);
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: resultStr.slice(0, 50000) });
          allToolResults.push({ name: toolName, result });
          yield { type: "tool_result", toolName, toolResult: result };
        } catch (err: any) {
          const errorStr = `Error: ${err.message}`;
          toolResultBlocks.push({ type: "tool_result", toolUseId: toolCallId, content: errorStr });
          yield { type: "tool_result", toolName, toolResult: errorStr };
        }
      }

      llmMessages.push({ role: "user", content: toolResultBlocks });
    }

    // Store assistant response
    this.state.phase = "reflect";
    const assistantMsg: Message = {
      id: randomUUID(), role: "assistant", content: finalResponse, timestamp: Date.now(),
    };
    session.messages.push(assistantMsg);
    session.updatedAt = Date.now();
    this.memory.pushMessage(session.id, assistantMsg);
    this.state.turnCount++;

    if (this.state.turnCount % 10 === 0) {
      this.autoFlushFromSession(session.id);
    }

    yield { type: "done", usage: { inputTokens: totalInputTokens, outputTokens: totalOutputTokens } };
  }

  // ─── Auto-flush to Vault (Phase 15.4) ──────────────────────────────

  private autoFlushFromSession(sessionId: string): void {
    const messages = this.memory.getSessionMessages(sessionId);
    if (messages.length < 4) return;

    // Extract facts heuristically from recent assistant messages
    const recent = messages.slice(-10).filter((m) => m.role === "assistant");
    const facts: { category: "preference" | "fact" | "decision" | "learning"; title: string; content: string }[] = [];

    for (const msg of recent) {
      const lines = msg.content.split("\n").filter((l) => l.trim().length > 10);
      for (const line of lines) {
        const lower = line.toLowerCase();
        // Detect decisional statements
        if (/\b(decided|chose|selected|prefer|configured|set up|created|installed)\b/i.test(lower) && line.length < 300) {
          facts.push({ category: "decision", title: line.slice(0, 80).trim(), content: line.trim() });
        }
        // Detect learnings / discovered facts
        else if (/\b(found that|discovered|turns out|note:|important:|remember:)\b/i.test(lower) && line.length < 300) {
          facts.push({ category: "learning", title: line.slice(0, 80).trim(), content: line.trim() });
        }
      }
    }

    if (facts.length > 0) {
      // Limit to avoid flooding vault — map internal categories to VaultNote schema
      const vaultFacts = facts.slice(0, 5).map((f) => ({
        title: f.title,
        content: f.content,
        category: (
          f.category === "decision" ? "methodology" :
          f.category === "learning" ? "expertise" :
          f.category === "fact" ? "expertise" : "preference"
        ) as "expertise" | "identity" | "preference" | "constraint" | "methodology",
      }));
      this.memory.autoFlushToVault(sessionId, vaultFacts);
    }
  }

  // ─── Compaction & Ingestion ───────────────────────────────────────

  async compactSession(sessionId: string): Promise<void> {
    const messages = this.memory.getSessionMessages(sessionId);
    if (messages.length < 20) return;

    const transcript = messages
      .slice(0, -10)
      .map((m) => `${m.role}: ${m.content.slice(0, 200)}`)
      .join("\n");

    const response = await this.llm.chat({
      model: this.config.model,
      messages: [
        {
          role: "user",
          content: `Summarize this conversation concisely, preserving key decisions, facts, and context:\n\n${transcript}`,
        },
      ],
      maxTokens: 2000,
    });

    const summary = response.content.find((b) => b.type === "text")?.text || "";
    this.memory.compactSession(sessionId, summary);

    // Auto-flush key facts to vault (Phase 15.4)
    this.autoFlushFromSession(sessionId);
  }

  ingestSession(sessionId: string): number {
    return this.memory.ingestSession(sessionId);
  }

  getState(): AgentState {
    return { ...this.state };
  }
}

// ─── Response Types ─────────────────────────────────────────────────────────

export interface AgentResponse {
  sessionId: string;
  content: string;
  thinking?: string;
  toolResults: { name: string; result: unknown }[];
  usage: { inputTokens: number; outputTokens: number };
}
