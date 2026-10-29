import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { listApps, me, platformApiOrigin } from "./api";

const mockFetch = vi.fn<typeof fetch>();

beforeEach(() => {
  vi.stubGlobal("fetch", mockFetch);
});
afterEach(() => {
  vi.restoreAllMocks();
  mockFetch.mockReset();
});

describe("platformApiOrigin", () => {
  it("defaults to same-origin (empty string)", () => {
    expect(platformApiOrigin()).toBe("");
  });
});

describe("me()", () => {
  it("calls /api/v1/me with credentials include", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ user: { id: "u1", username: "alice", email: "a@b.com", role: "member" } }), {
        status: 200,
        headers: { "content-type": "application/json" },
      }),
    );

    const user = await me();

    expect(mockFetch).toHaveBeenCalledOnce();
    const [url, init] = mockFetch.mock.calls[0];
    expect(url).toBe("/api/v1/me");
    expect((init as RequestInit).credentials).toBe("include");
    expect(user).toMatchObject({ id: "u1", username: "alice" });
  });

  it("returns null on a 401", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ error: "not_authenticated" }), {
        status: 401,
        headers: { "content-type": "application/json" },
      }),
    );
    expect(await me()).toBeNull();
  });

  it("returns null when the network is unreachable", async () => {
    mockFetch.mockRejectedValueOnce(new TypeError("Failed to fetch"));
    expect(await me()).toBeNull();
  });
});

describe("listApps()", () => {
  const releaseApp = {
    id: "nexus-draw",
    name: "Nexus Draw",
    description: "Collaborative drawing",
    path: "/draw",
    delivery: "framed" as const,
    health: "healthy" as const,
    surface: "user-app" as const,
    releaseState: "released" as const,
    maturity: "stable" as const,
    reviewStatus: "approved" as const,
    visibility: "default" as const,
  };

  it("calls /api/v1/apps with credentials include", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ apps: [releaseApp] }), {
        status: 200,
        headers: { "content-type": "application/json" },
      }),
    );

    await listApps();

    expect(mockFetch).toHaveBeenCalledOnce();
    const [url, init] = mockFetch.mock.calls[0];
    expect(url).toBe("/api/v1/apps");
    expect((init as RequestInit).credentials).toBe("include");
  });

  it("returns normalized app entries", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ apps: [releaseApp] }), {
        status: 200,
        headers: { "content-type": "application/json" },
      }),
    );

    const apps = await listApps();
    // listApps prepends the pinned shell-native apps, so assert the registry
    // entry is present and normalized rather than that it is the only entry.
    const draw = apps.find((a) => a.id === "nexus-draw");
    expect(draw).toBeDefined();
    expect(draw!.path).toBe("/draw");
  });
});
