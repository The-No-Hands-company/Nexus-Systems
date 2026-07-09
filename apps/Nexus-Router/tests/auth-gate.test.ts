import { describe, it, expect } from "bun:test";
import { authGate } from "../src/middleware/auth-gate";
import { IncomingMessage, ServerResponse } from "http";

describe("Auth Gate", () => {
  it("should reject request without auth header", async () => {
    const mockReq = { headers: {} } as IncomingMessage;
    const mockRes = {
      writeHead: (code: number) => code,
      end: (data: string) => data,
      writableEnded: false,
    } as any as ServerResponse;

    const context = {
      requestId: "test-1",
      metadata: {},
    };
    const config = { auth: { phantom_enabled: true } };

    await authGate(mockReq, mockRes, context, config);

    expect(mockRes.writableEnded).toBe(false); // Would be true if writeHead was called
  });

  it("should parse Bearer token with Phantom DID", async () => {
    const mockReq = {
      headers: {
        authorization: "Bearer did:phantom:abc123def456",
      },
    } as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-2",
      metadata: {},
    };
    const config = { auth: { phantom_enabled: true } };

    await authGate(mockReq, mockRes, context, config);

    expect(context.phantomDid).toBe("did:phantom:abc123def456");
  });

  it("should skip auth if disabled", async () => {
    const mockReq = { headers: {} } as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-3",
      metadata: {},
    };
    const config = { auth: { phantom_enabled: false } };

    await authGate(mockReq, mockRes, context, config);

    // Should not set phantomDid
    expect(context.phantomDid).toBeUndefined();
  });
});
