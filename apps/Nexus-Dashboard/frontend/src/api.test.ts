import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ApiError, listApps, me, moveMailMessage, platformApiOrigin, session } from "./api";

const mockFetch = vi.fn<typeof fetch>();

beforeEach(() => {
  // Stub global fetch for tests
  (globalThis as any).fetch = mockFetch;
});
afterEach(() => {
  vi.restoreAllMocks();
  vi.useRealTimers();
  mockFetch.mockReset();
});

describe("platformApiOrigin", () => {
  it("defaults to same-origin (empty string)", () => {
    expect(platformApiOrigin()).toBe("");
  });
});

describe("me()", () => {
  it("calls /ipa/v1/me with credentials include", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ user: { id: "u1", username: "alice", email: "a@b.com", role: "member" } }), {
        status: 200,
        headers: { "content-type": "application/json" },
      }),
    );

    const user = await me();

    expect(mockFetch).toHaveBeenCalledOnce();
    expect(mockFetch).toHaveBeenCalledWith(
      "/ipa/v1/me",
      expect.objectContaining({ credentials: "include" }),
    );
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

describe("session()", () => {
  const json = (body: unknown, status = 200) =>
    new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
  const aliceResponse = () =>
    json({ user: { id: "u1", username: "alice", email: "a@b.com", role: "member" } });

  it("is signed-in when the server returns the user", async () => {
    mockFetch.mockResolvedValueOnce(aliceResponse());
    expect(await session()).toMatchObject({ status: "signed-in", user: { username: "alice" } });
  });

  it("is signed-out only when the server answers 401", async () => {
    mockFetch.mockResolvedValueOnce(json({ error: "unauthenticated" }, 401));
    expect(await session()).toEqual({ status: "signed-out" });
  });

  it("is unreachable, not signed-out, when the server errors", async () => {
    mockFetch.mockResolvedValueOnce(json({ error: "bad_gateway" }, 502));
    expect(await session()).toEqual({ status: "unreachable" });
  });

  it("retries a dropped connection and recovers", async () => {
    vi.useFakeTimers();
    mockFetch.mockRejectedValueOnce(new TypeError("Failed to fetch")).mockResolvedValueOnce(aliceResponse());
    const pending = session();
    vi.advanceTimersByTime(400);
    expect(await pending).toMatchObject({ status: "signed-in" });
    expect(mockFetch).toHaveBeenCalledTimes(2);
  });

  it("reports unreachable after the retries are spent", async () => {
    vi.useFakeTimers();
    mockFetch.mockRejectedValue(new TypeError("Failed to fetch"));
    const pending = session();
    vi.advanceTimersByTime(400);
vi.advanceTimersByTime(1200);
    expect(await pending).toEqual({ status: "unreachable" });
    expect(mockFetch).toHaveBeenCalledTimes(3);
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

  it("calls /ipa/v1/apps with credentials include", async () => {
    mockFetch.mockResolvedValueOnce(
      new Response(JSON.stringify({ apps: [releaseApp] }), {
        status: 200,
        headers: { "content-type": "application/json" },
      }),
    );

    await listApps();

    expect(mockFetch).toHaveBeenCalledOnce();
    expect(mockFetch).toHaveBeenCalledWith(
      "/ipa/v1/apps",
      expect.objectContaining({ credentials: "include" }),
    );
  });

  describe("moveMailMessage()", () => {
    it("uses the same-origin Dashboard Mail proxy", async () => {
      mockFetch.mockResolvedValueOnce(new Response(null, { status: 204 }));

      await moveMailMessage("m-1", "trash");

      expect(mockFetch).toHaveBeenCalledWith(
        "/ipa/mail/messages/m-1/folder",
        expect.objectContaining({ credentials: "same-origin", method: "POST" }),
      );
    });
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

  it("fails with timeout when the platform facade does not answer in time", async () => {
    vi.useFakeTimers();
    mockFetch.mockImplementationOnce(
      (_url, init) =>
        new Promise<Response>((_resolve, reject) => {
          init?.signal?.addEventListener("abort", () => {
            reject(new DOMException("signal timed out", "AbortError"));
          });
        }),
    );

    const pending = listApps();
    const rejection = expect(pending).rejects.toEqual(
      new ApiError("timeout", "The server took too long to respond. Please try again."),
    );
    await vi.advanceTimersByTimeAsync(10_000);

    await rejection;
  });
});
