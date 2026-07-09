import { describe, it, expect } from "bun:test";
import { ipRouter } from "../src/middleware/ip-router";
import { IncomingMessage, ServerResponse } from "http";

describe("IP Router", () => {
  it("should determine EU region from CIDR", async () => {
    const mockReq = { headers: {} } as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-1",
      clientIp: "2.1.2.3",
      metadata: {},
    };
    const config = {
      geography: {
        default_region: "us-east-1",
        regions: {
          eu: {
            cidrs: ["2.0.0.0/8"],
            cloud_url: "http://nexus-eu.local:8787",
          },
        },
      },
    };

    await ipRouter(mockReq, mockRes, context, config);

    expect(context.region).toBe("eu");
    expect(context.metadata.region).toBe("eu");
  });

  it("should fall back to default region", async () => {
    const mockReq = { headers: {} } as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-2",
      clientIp: "10.1.2.3",
      metadata: {},
    };
    const config = {
      geography: {
        default_region: "us-east-1",
        regions: {
          eu: {
            cidrs: ["2.0.0.0/8"],
            cloud_url: "http://nexus-eu.local:8787",
          },
        },
      },
    };

    await ipRouter(mockReq, mockRes, context, config);

    expect(context.region).toBe("us-east-1");
  });

  it("should handle missing geography config", async () => {
    const mockReq = { headers: {} } as IncomingMessage;
    const mockRes = {} as ServerResponse;
    const context = {
      requestId: "test-3",
      clientIp: "1.1.1.1",
      metadata: {},
    };
    const config = { geography: undefined };

    await ipRouter(mockReq, mockRes, context, config);

    // Should not error and region should remain undefined
    expect(context.region).toBeUndefined();
  });
});
