// LLM Provider abstraction — model-agnostic interface
// Phase 1.5: Streaming responses
// Phase 1.6: Rate limiting enforcement
// Phase 1.7: Extended thinking pass-through
// Phase 2.1: Embedding API support

import type {
  LLMRequest,
  LLMResponse,
  LLMMessage,
  LLMToolDef,
  LLMContentBlock,
  StreamEvent,
} from "../core/types.js";

export interface LLMProvider {
  name: string;
  chat(request: LLMRequest): Promise<LLMResponse>;
  streamChat?(request: LLMRequest): AsyncIterable<StreamEvent>;
  embed?(texts: string[], model?: string): Promise<number[][]>;
  listModels?(): Promise<string[]>;
}

// ─── Rate Limiter (Phase 1.6) ───────────────────────────────────────────

export interface RateLimitConfig {
  requestsPerMinute: number;
  tokensPerMinute: number;
  maxRetries: number;
  backoffMs: number;
}

const DEFAULT_RATE_LIMIT: RateLimitConfig = {
  requestsPerMinute: 60,
  tokensPerMinute: 100000,
  maxRetries: 3,
  backoffMs: 1000,
};

export class RateLimiter {
  private config: RateLimitConfig;
  private requestTimestamps: number[] = [];
  private tokenTimestamps: { ts: number; tokens: number }[] = [];

  constructor(config: Partial<RateLimitConfig> = {}) {
    this.config = { ...DEFAULT_RATE_LIMIT, ...config };
  }

  private pruneWindow(): void {
    const cutoff = Date.now() - 60000;
    this.requestTimestamps = this.requestTimestamps.filter(ts => ts > cutoff);
    this.tokenTimestamps = this.tokenTimestamps.filter(e => e.ts > cutoff);
  }

  private currentRPM(): number {
    this.pruneWindow();
    return this.requestTimestamps.length;
  }

  private currentTPM(): number {
    this.pruneWindow();
    return this.tokenTimestamps.reduce((sum, e) => sum + e.tokens, 0);
  }

  async waitForCapacity(): Promise<void> {
    while (this.currentRPM() >= this.config.requestsPerMinute) {
      const oldest = this.requestTimestamps[0];
      const waitMs = Math.max(oldest + 60000 - Date.now() + 50, 100);
      await new Promise(r => setTimeout(r, waitMs));
    }
  }

  recordRequest(): void {
    this.requestTimestamps.push(Date.now());
  }

  recordTokens(tokens: number): void {
    this.tokenTimestamps.push({ ts: Date.now(), tokens });
  }

  async executeWithRetry<T>(fn: () => Promise<T>): Promise<T> {
    let lastError: Error | undefined;
    for (let attempt = 0; attempt <= this.config.maxRetries; attempt++) {
      await this.waitForCapacity();
      this.recordRequest();
      try {
        return await fn();
      } catch (err: any) {
        lastError = err;
        const isRateLimit = err.status === 429 || err.message?.includes("rate_limit");
        if (!isRateLimit || attempt === this.config.maxRetries) throw err;
        const backoff = this.config.backoffMs * Math.pow(2, attempt);
        await new Promise(r => setTimeout(r, backoff));
      }
    }
    throw lastError;
  }

  getStats(): { rpm: number; tpm: number; limits: RateLimitConfig } {
    return { rpm: this.currentRPM(), tpm: this.currentTPM(), limits: this.config };
  }
}

// ─── Mock LLM Provider (Phase 9, 10.7 dev mode) ────────────────────────────

export class MockLLMProvider implements LLMProvider {
  name = "mock";

  async chat(request: LLMRequest): Promise<LLMResponse> {
    const last = request.messages[request.messages.length - 1];
    const userText = last?.role === "user"
      ? (Array.isArray(last.content)
          ? last.content.filter((b: any) => b.type === "text").map((b: any) => b.text).join(" ")
          : String(last.content))
      : "(empty)";

    // Simulate a short think time
    await new Promise(r => setTimeout(r, 80));

    return {
      content: `[MOCK] You said: "${userText.slice(0, 200)}"`,
      thinking: undefined,
      toolUses: [],
      usage: { inputTokens: 10, outputTokens: 20 },
      stopReason: "end_turn",
      model: request.model,
    };
  }

  async *streamChat(request: LLMRequest): AsyncIterable<StreamEvent> {
    const last = request.messages[request.messages.length - 1];
    const userText = last?.role === "user"
      ? (Array.isArray(last.content)
          ? last.content.filter((b: any) => b.type === "text").map((b: any) => b.text).join(" ")
          : String(last.content))
      : "(empty)";

    await new Promise(r => setTimeout(r, 60));
    const reply = `[MOCK DEV] Echo: "${userText.slice(0, 200)}"`;
    // Stream word-by-word
    for (const word of reply.split(" ")) {
      yield { type: "text_delta", content: word + " " };
      await new Promise(r => setTimeout(r, 12));
    }
    yield { type: "done" };
  }
}

// ─── Anthropic Provider ─────────────────────────────────────────────────

export class AnthropicProvider implements LLMProvider {
  name = "anthropic";
  private apiKey: string;
  private sdk: any;

  constructor(apiKey: string) {
    this.apiKey = apiKey;
  }

  private async getSdk() {
    if (!this.sdk) {
      const { default: Anthropic } = await import("@anthropic-ai/sdk");
      this.sdk = new Anthropic({ apiKey: this.apiKey });
    }
    return this.sdk;
  }

  private buildParams(request: LLMRequest): any {
    const systemMsg = request.messages.find((m) => m.role === "system");
    const nonSystemMsgs = request.messages.filter((m) => m.role !== "system");

    const params: any = {
      model: request.model.replace("anthropic/", ""),
      max_tokens: request.maxTokens || 8192,
      messages: nonSystemMsgs.map((m) => ({
        role: m.role as "user" | "assistant",
        content: typeof m.content === "string" ? m.content : m.content,
      })),
    };

    if (systemMsg) {
      params.system =
        typeof systemMsg.content === "string"
          ? systemMsg.content
          : JSON.stringify(systemMsg.content);
    }

    if (request.tools && request.tools.length > 0) {
      params.tools = request.tools.map((t) => ({
        name: t.name,
        description: t.description,
        input_schema: t.inputSchema,
      }));
    }

    if (request.temperature !== undefined) {
      params.temperature = request.temperature;
    }

    // Extended thinking (Phase 1.7)
    if (request.thinking && request.thinking !== "off") {
      const budgetMap: Record<string, number> = {
        minimal: 1024,
        low: 4096,
        medium: 10000,
        high: 32000,
      };
      params.thinking = {
        type: "enabled",
        budget_tokens: budgetMap[request.thinking] || 10000,
      };
      // Extended thinking requires higher max_tokens
      params.max_tokens = Math.max(params.max_tokens, params.thinking.budget_tokens + 4096);
    }

    return params;
  }

  async chat(request: LLMRequest): Promise<LLMResponse> {
    const sdk = await this.getSdk();
    const params = this.buildParams(request);
    const response = await sdk.messages.create(params);

    const contentBlocks: LLMContentBlock[] = response.content.map(
      (block: any) => {
        if (block.type === "thinking") {
          return { type: "thinking" as const, text: block.thinking };
        }
        if (block.type === "text") {
          return { type: "text" as const, text: block.text };
        }
        if (block.type === "tool_use") {
          return {
            type: "tool_use" as const,
            id: block.id,
            name: block.name,
            input: block.input,
          };
        }
        return { type: "text" as const, text: JSON.stringify(block) };
      },
    );

    return {
      id: response.id,
      content: contentBlocks,
      model: response.model,
      usage: {
        inputTokens: response.usage.input_tokens,
        outputTokens: response.usage.output_tokens,
      },
      stopReason: response.stop_reason,
    };
  }

  // Phase 1.5: Streaming
  async *streamChat(request: LLMRequest): AsyncIterable<StreamEvent> {
    const sdk = await this.getSdk();
    const params = { ...this.buildParams(request), stream: true };

    const stream = await sdk.messages.stream(params);

    for await (const event of stream) {
      if (event.type === "content_block_delta") {
        const delta = event.delta as any;
        if (delta.type === "text_delta") {
          yield { type: "text_delta", content: delta.text };
        } else if (delta.type === "thinking_delta") {
          yield { type: "thinking", content: delta.thinking };
        }
      } else if (event.type === "content_block_start") {
        const block = event.content_block as any;
        if (block.type === "tool_use") {
          yield { type: "tool_start", toolName: block.name, content: block.id };
        }
      } else if (event.type === "message_stop") {
        yield { type: "done" };
      }
    }

    const finalMessage = await stream.finalMessage();
    yield {
      type: "done",
      usage: {
        inputTokens: finalMessage.usage.input_tokens,
        outputTokens: finalMessage.usage.output_tokens,
      },
    };
  }

  // Phase 2.1: Embeddings — Anthropic doesn't have native embeddings,
  // so we use a description-based approach or fall through to OpenAI/Ollama
  async embed(texts: string[], _model?: string): Promise<number[][]> {
    // Anthropic has no embedding API — throw so the router can fall back
    throw new Error("Anthropic does not provide an embedding API. Use OpenAI or Ollama for embeddings.");
  }
}

// ─── OpenAI-Compatible Provider ──────────────────────────────────────────

export class OpenAICompatibleProvider implements LLMProvider {
  name: string;
  private apiKey: string;
  private baseUrl?: string;
  private sdk: any;

  constructor(apiKey: string, baseUrl?: string, name = "openai") {
    this.apiKey = apiKey;
    this.baseUrl = baseUrl;
    this.name = name;
  }

  private async getSdk() {
    if (!this.sdk) {
      const { default: OpenAI } = await import("openai");
      this.sdk = new OpenAI({
        apiKey: this.apiKey,
        ...(this.baseUrl && { baseURL: this.baseUrl }),
      });
    }
    return this.sdk;
  }

  async chat(request: LLMRequest): Promise<LLMResponse> {
    const sdk = await this.getSdk();

    const params: any = {
      model: request.model.replace(/^(openai|ollama)\//, ""),
      max_tokens: request.maxTokens || 8192,
      messages: request.messages.map((m) => ({
        role: m.role,
        content:
          typeof m.content === "string"
            ? m.content
            : JSON.stringify(m.content),
      })),
    };

    if (request.tools && request.tools.length > 0) {
      params.tools = request.tools.map((t) => ({
        type: "function",
        function: {
          name: t.name,
          description: t.description,
          parameters: t.inputSchema,
        },
      }));
    }

    if (request.temperature !== undefined) {
      params.temperature = request.temperature;
    }

    const response = await sdk.chat.completions.create(params);
    const choice = response.choices[0];

    const contentBlocks: LLMContentBlock[] = [];

    if (choice.message.content) {
      contentBlocks.push({ type: "text", text: choice.message.content });
    }

    if (choice.message.tool_calls) {
      for (const tc of choice.message.tool_calls) {
        contentBlocks.push({
          type: "tool_use",
          id: tc.id,
          name: tc.function.name,
          input: JSON.parse(tc.function.arguments || "{}"),
        });
      }
    }

    return {
      id: response.id,
      content: contentBlocks,
      model: response.model,
      usage: {
        inputTokens: response.usage?.prompt_tokens || 0,
        outputTokens: response.usage?.completion_tokens || 0,
      },
      stopReason: choice.finish_reason,
    };
  }

  // Phase 1.5: Streaming for OpenAI-compatible
  async *streamChat(request: LLMRequest): AsyncIterable<StreamEvent> {
    const sdk = await this.getSdk();

    const params: any = {
      model: request.model.replace(/^(openai|ollama)\//, ""),
      max_tokens: request.maxTokens || 8192,
      stream: true,
      messages: request.messages.map((m) => ({
        role: m.role,
        content:
          typeof m.content === "string"
            ? m.content
            : JSON.stringify(m.content),
      })),
    };

    if (request.temperature !== undefined) {
      params.temperature = request.temperature;
    }

    const stream = await sdk.chat.completions.create(params);

    for await (const chunk of stream) {
      const delta = chunk.choices?.[0]?.delta;
      if (delta?.content) {
        yield { type: "text_delta", content: delta.content };
      }
      if (chunk.choices?.[0]?.finish_reason) {
        yield {
          type: "done",
          usage: {
            inputTokens: chunk.usage?.prompt_tokens || 0,
            outputTokens: chunk.usage?.completion_tokens || 0,
          },
        };
      }
    }
  }

  // Phase 2.1: Embeddings via OpenAI-compatible API
  async embed(texts: string[], model?: string): Promise<number[][]> {
    const sdk = await this.getSdk();
    const embModel = model || (this.name === "ollama" ? "nomic-embed-text" : "text-embedding-3-small");
    const response = await sdk.embeddings.create({ model: embModel, input: texts });
    return response.data.map((d: any) => d.embedding);
  }
}

// ─── LLM Router ─────────────────────────────────────────────────────────

export class LLMRouter {
  private providers = new Map<string, LLMProvider>();
  private rateLimiter: RateLimiter;

  constructor(rateLimitConfig?: Partial<RateLimitConfig>) {
    this.rateLimiter = new RateLimiter(rateLimitConfig);
  }

  registerProvider(prefix: string, provider: LLMProvider): void {
    this.providers.set(prefix, provider);
  }

  private resolveProvider(model: string): LLMProvider {
    const prefix = model.split("/")[0];
    const provider = this.providers.get(prefix);
    if (!provider) {
      throw new Error(
        `No LLM provider registered for prefix "${prefix}". Available: ${[...this.providers.keys()].join(", ")}`,
      );
    }
    return provider;
  }

  async chat(request: LLMRequest): Promise<LLMResponse> {
    const provider = this.resolveProvider(request.model);
    return this.rateLimiter.executeWithRetry(async () => {
      const response = await provider.chat(request);
      this.rateLimiter.recordTokens(response.usage.inputTokens + response.usage.outputTokens);
      return response;
    });
  }

  async *streamChat(request: LLMRequest): AsyncIterable<StreamEvent> {
    const provider = this.resolveProvider(request.model);
    await this.rateLimiter.waitForCapacity();
    this.rateLimiter.recordRequest();
    if (!provider.streamChat) {
      const response = await provider.chat(request);
      this.rateLimiter.recordTokens(response.usage.inputTokens + response.usage.outputTokens);
      const text = response.content
        .filter((b) => b.type === "text")
        .map((b) => b.text)
        .join("");
      if (text) yield { type: "text_delta", content: text };
      yield { type: "done", usage: response.usage };
      return;
    }
    yield* provider.streamChat(request);
  }

  async chatWithFallback(
    request: LLMRequest,
    fallbackModel?: string,
  ): Promise<LLMResponse> {
    try {
      return await this.chat(request);
    } catch (err) {
      if (fallbackModel) {
        console.warn(
          `Primary model ${request.model} failed, trying fallback ${fallbackModel}`,
        );
        return this.chat({ ...request, model: fallbackModel });
      }
      throw err;
    }
  }

  /** Embed text using any available provider with embedding support (Phase 2.1) */
  async embed(texts: string[], model?: string): Promise<number[][]> {
    const tryOrder = ["openai", "ollama", ...this.providers.keys()];
    const tried = new Set<string>();
    for (const prefix of tryOrder) {
      if (tried.has(prefix)) continue;
      tried.add(prefix);
      const provider = this.providers.get(prefix);
      if (!provider?.embed) continue;
      try {
        return await this.rateLimiter.executeWithRetry(() => provider.embed!(texts, model));
      } catch {
        // Try next provider
      }
    }
    throw new Error("No embedding provider available. Configure OpenAI or Ollama API key.");
  }

  /** Create an embedding function compatible with MemoryManager.setEmbeddingFn */
  createEmbeddingFn(): ((text: string) => Promise<number[]>) | null {
    const hasEmbed = [...this.providers.values()].some(p => p.embed);
    if (!hasEmbed) return null;
    return async (text: string) => {
      const results = await this.embed([text]);
      return results[0];
    };
  }

  getRateLimitStats() {
    return this.rateLimiter.getStats();
  }

  getProviders(): string[] {
    return [...this.providers.keys()];
  }
}

// ─── Factory ────────────────────────────────────────────────────────────

export function createRouter(rateLimitConfig?: Partial<RateLimitConfig>): LLMRouter {
  const router = new LLMRouter(rateLimitConfig);

  // ── Mock provider (always registered; used when model starts with "mock/") ─
  router.registerProvider("mock", new MockLLMProvider());

  // ── Nexus AI (preferred engine when NEXUS_AI_URL is set) ──────────────────
  // Nexus AI exposes a /v1/chat/completions OpenAI-compatible endpoint so we
  // reuse the existing OpenAICompatibleProvider — no custom adapter needed.
  const nexusAiUrl = process.env.NEXUS_AI_URL;
  if (nexusAiUrl) {
    const nexusAiKey = process.env.NEXUS_AI_API_KEY || "nexus-ai";
    // Register under both "nexus_ai" and "nexus-ai" prefixes for flexibility.
    const nexusProvider = new OpenAICompatibleProvider(nexusAiKey, `${nexusAiUrl.replace(/\/$/, "")}/v1`, "nexus-ai");
    router.registerProvider("nexus_ai", nexusProvider);
    router.registerProvider("nexus-ai", nexusProvider);
  }

  if (process.env.ANTHROPIC_API_KEY) {
    router.registerProvider(
      "anthropic",
      new AnthropicProvider(process.env.ANTHROPIC_API_KEY),
    );
  }
  if (process.env.OPENAI_API_KEY) {
    router.registerProvider(
      "openai",
      new OpenAICompatibleProvider(process.env.OPENAI_API_KEY),
    );
  }

  const ollamaUrl = process.env.OLLAMA_BASE_URL || "http://localhost:11434";
  router.registerProvider(
    "ollama",
    new OpenAICompatibleProvider("ollama", `${ollamaUrl}/v1`, "ollama"),
  );

  return router;
}
