import { describe, it, expect } from "bun:test";
import { aiProviderRouter } from "../src/middleware/ai-provider-router";
import { IncomingMessage, ServerResponse } from "http";

describe("AI Provider Router", () => {
  it("should skip non-AI requests", async () => {
    const mockReq = {
      url: "/api/host",
      headers: {},
    } as unknown as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-1",
      metadata: {},
    };
    const config = { ai_providers: { default_model: "ollama" } };

    await aiProviderRouter(mockReq, mockRes, context, config);

    expect(context.metadata.ai_provider).toBeUndefined();
  });

  it("should route legal tasks to Claude", async () => {
    const mockReq = {
      url: "/api/chat?task=legal:review",
      headers: {},
    } as unknown as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-2",
      metadata: {},
    };
    const config = {
      ai_providers: {
        default_model: "ollama",
        routing_rules: [
          { pattern: "legal:*", model: "claude", fallback: "gpt-4" },
          { pattern: "*", model: "ollama", fallback: "groq" },
        ],
      },
    };

    await aiProviderRouter(mockReq, mockRes, context, config);

    expect(context.metadata.ai_provider).toBe("claude");
  });

  it("should route code tasks to Groq", async () => {
    const mockReq = {
      url: "/api/chat?task=code:generate",
      headers: {},
    } as unknown as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-3",
      metadata: {},
    };
    const config = {
      ai_providers: {
        default_model: "ollama",
        routing_rules: [
          { pattern: "code:*", model: "groq", fallback: "ollama" },
          { pattern: "*", model: "ollama", fallback: "groq" },
        ],
      },
    };

    await aiProviderRouter(mockReq, mockRes, context, config);

    // Should route to groq based on pattern
    expect(context.metadata.ai_provider).toBe("groq");
  });

  it("should default to ollama", async () => {
    const mockReq = {
      url: "/api/chat",
      headers: {},
    } as unknown as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-4",
      metadata: {},
    };
    const config = {
      ai_providers: {
        default_model: "ollama",
        routing_rules: [
          { pattern: "*", model: "ollama", fallback: "groq" },
        ],
      },
    };

    await aiProviderRouter(mockReq, mockRes, context, config);

    expect(context.metadata.ai_provider).toBe("ollama");
  });
});
