import { describe, expect, it, vi } from "vitest";
import { calendarApi } from "./calendar-api";

function response(body: BodyInit | null, status: number, contentType = "application/json") {
  return new Response(body, { status, headers: { "content-type": contentType } });
}

describe("calendarApi failures", () => {
  it.each([
    ["network", () => Promise.reject(new TypeError("offline")), 0, "offline"],
    ["non-json gateway", () => Promise.resolve(response("bad gateway", 503, "text/html")), 503, "unavailable"],
    ["validation envelope", () => Promise.resolve(response(JSON.stringify({ error: "title is required" }), 400)), 400, "validation"],
    ["unauthenticated", () => Promise.resolve(response(JSON.stringify({ error: "not authenticated" }), 401)), 401, "unauthenticated"],
    ["not found", () => Promise.resolve(response(JSON.stringify({ error: "not found" }), 404)), 404, "not_found"],
    ["service unavailable", () => Promise.resolve(response(JSON.stringify({ error: "calendar_unavailable" }), 503)), 503, "unavailable"],
  ])("throws stable %s errors rather than returning an empty list", async (_name, fetcher, status, reason) => {
    const api = calendarApi("/ipa/calendar", fetcher as typeof fetch);
    await expect(api.listEvents("2026-08-01", "2026-08-31")).rejects.toMatchObject({ status, reason });
  });

  it("uses the runtime-provided proxy base and credentials", async () => {
    const fetcher = vi.fn(async () => response(JSON.stringify({ events: [] }), 200));
    await calendarApi("/ipa/calendar", fetcher).listEvents("2026-08-01", "2026-08-31");
    expect(fetcher).toHaveBeenCalledWith(
      "/ipa/calendar/events?from=2026-08-01&to=2026-08-31",
      expect.objectContaining({ credentials: "same-origin" }),
    );
  });
});
