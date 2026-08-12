import { describe, it, expect } from "bun:test";
import { getUserIdByDid, getDidByUserId } from "../src/index";

describe("phantom-did-client (mock)", () => {
  it("getUserIdByDid calls did-mapper and returns userId", async () => {
    let calledUrl = "";
    // mock fetch
    globalThis.fetch = async (input: RequestInfo) => {
      calledUrl = String(input);
      return new Response(JSON.stringify({ userId: "user-123" }), {
        status: 200,
        headers: { "content-type": "application/json" },
      });
    };

    const res = await getUserIdByDid("did:example:abc");
    expect(res).toBe("user-123");

    const expectedBase = process.env.DID_MAPPER_URL ?? "http://localhost:3000";
    expect(calledUrl).toBe(expectedBase + `/user-id?did=${encodeURIComponent("did:example:abc")}`);
  });

  it("getDidByUserId returns did", async () => {
    globalThis.fetch = async (_) =>
      new Response(JSON.stringify({ did: "did:example:abc" }), {
        status: 200,
        headers: { "content-type": "application/json" },
      });

    const res = await getDidByUserId("user-123");
    expect(res).toBe("did:example:abc");
  });
});
