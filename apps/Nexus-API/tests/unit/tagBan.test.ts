import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import express from "express";
import http from "node:http";
import type { AddressInfo } from "node:net";
import { banTag, unbanTag, isTagBanned, listTagBans, clearTagBans, MAX_BAN_MS } from "../../src/lib/tagBan";
import { apiBanMiddleware } from "../../src/middleware/tagBan";

const TAG_A = "AAAAAAAAAAAAAAAAAAAAAA";
const TAG_B = "BBBBBBBBBBBBBBBBBBBBBB";

describe("in-memory tag bans", () => {
  beforeEach(() => clearTagBans());
  afterEach(() => vi.useRealTimers());

  it("bans and unbans a tag", () => {
    expect(isTagBanned(TAG_A)).toBe(false);
    banTag(TAG_A);
    expect(isTagBanned(TAG_A)).toBe(true);
    expect(isTagBanned(TAG_B)).toBe(false);
    expect(unbanTag(TAG_A)).toBe(true);
    expect(isTagBanned(TAG_A)).toBe(false);
  });

  it("caps any ban at 24 hours, however long was asked for", () => {
    vi.useFakeTimers();
    const start = Date.now();
    const expiresAt = banTag(TAG_A, 30 * 24 * 60 * 60 * 1000);
    expect(expiresAt - start).toBeLessThanOrEqual(MAX_BAN_MS);
    vi.advanceTimersByTime(MAX_BAN_MS - 1000);
    expect(isTagBanned(TAG_A)).toBe(true);
    vi.advanceTimersByTime(2000);
    expect(isTagBanned(TAG_A)).toBe(false);
  });

  it("expires shorter bans on schedule and drops them from the list", () => {
    vi.useFakeTimers();
    banTag(TAG_A, 60_000);
    expect(listTagBans().map((b) => b.tag)).toEqual([TAG_A]);
    vi.advanceTimersByTime(61_000);
    expect(isTagBanned(TAG_A)).toBe(false);
    expect(listTagBans()).toEqual([]);
  });

  it("refuses to ban the shared 'unknown' tag (that would ban everyone)", () => {
    expect(() => banTag("unknown")).toThrow();
  });
});

describe("apiBanMiddleware", () => {
  beforeEach(() => clearTagBans());

  async function get(tag: string | undefined): Promise<number> {
    const app = express();
    app.use(apiBanMiddleware);
    app.get("/x", (_req, res) => res.send("ok"));
    const server = http.createServer(app);
    await new Promise<void>((r) => server.listen(0, "127.0.0.1", r));
    const port = (server.address() as AddressInfo).port;
    try {
      return await new Promise<number>((resolve, reject) => {
        const req = http.request({ host: "127.0.0.1", port, path: "/x", headers: tag ? { "x-nexus-client-tag": tag } : {} },
          (res) => { res.resume(); resolve(res.statusCode ?? 0); });
        req.on("error", reject);
        req.end();
      });
    } finally { server.close(); }
  }

  it("403s a banned tag and passes everyone else", async () => {
    banTag(TAG_A);
    expect(await get(TAG_A)).toBe(403);
    expect(await get(TAG_B)).toBe(200);
    expect(await get(undefined)).toBe(200);
  });

  it("a ban cannot be dodged or applied through x-forwarded-for", async () => {
    banTag(TAG_A);
    // The address header is not consulted at all.
    const app = express();
    app.use(apiBanMiddleware);
    app.get("/x", (_req, res) => res.send("ok"));
    const server = http.createServer(app);
    await new Promise<void>((r) => server.listen(0, "127.0.0.1", r));
    const port = (server.address() as AddressInfo).port;
    const status = await new Promise<number>((resolve, reject) => {
      const req = http.request({ host: "127.0.0.1", port, path: "/x", headers: { "x-forwarded-for": TAG_A } },
        (res) => { res.resume(); resolve(res.statusCode ?? 0); });
      req.on("error", reject); req.end();
    });
    server.close();
    expect(status).toBe(200);
  });
});
