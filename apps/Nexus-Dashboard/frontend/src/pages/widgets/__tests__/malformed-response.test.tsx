import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Today from "../Today";
import Unread from "../Unread";
import Activity from "../Activity";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

beforeEach(() => {
  vi.restoreAllMocks();
});

/**
 * A 200 with the wrong shape must land in the ERROR state, never crash the
 * render (`undefined.length`) and never silently coerce to an empty list —
 * either of those turns "we could not understand the answer" into either a
 * blank page or an indistinguishable-from-quiet-day empty widget.
 */
describe("widgets survive a malformed 200 response", () => {
  it("Today: an object missing `events` reaches the error state without throwing", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ notEvents: [] })));

    expect(() => render(<Today />)).not.toThrow();
    await waitFor(() => expect(screen.getByText("Calendar unavailable")).toBeTruthy());
    expect(screen.queryByText("Nothing scheduled today")).toBeNull();
  });

  it("Unread: an object missing `messages` reaches the error state without throwing", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ notMessages: [] })));

    expect(() => render(<Unread />)).not.toThrow();
    await waitFor(() => expect(screen.getByText("Mail unavailable")).toBeTruthy());
    expect(screen.queryByText("No unread mail")).toBeNull();
  });

  it("Unread: accepts the bare-array convention the rest of api.ts uses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse([
      { id: "m1", thread_id: "t1", subject: "Hi", from: "a@b.com", received_at: "2026-08-27T00:00:00Z", seen: false, flagged: false, snippet: null },
    ])));

    render(<MemoryRouter><Unread /></MemoryRouter>);
    // Unread shows "count plus flagged" per the spec, not a per-message
    // subject line for every unread message — this one is unflagged, so only
    // the count is asserted.
    await waitFor(() => expect(screen.getByText("1")).toBeTruthy());
  });

  it("Activity: an object missing `notifications` reaches the error state without throwing", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ notNotifications: [] })));

    expect(() => render(<Activity />)).not.toThrow();
    await waitFor(() => expect(screen.getByText("Notifications unavailable")).toBeTruthy());
    expect(screen.queryByText("Nothing recent")).toBeNull();
  });
});
