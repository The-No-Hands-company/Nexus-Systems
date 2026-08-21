import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, waitFor, act, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import NotificationBell from "./NotificationBell";

/**
 * The bell talks to the network, so every test here stubs fetch. A test that
 * let a real fetch through would be asserting on whatever the dev machine
 * happens to be running, which is how a suite goes green against nothing.
 */
function stubFetch(handler: (url: string, init?: RequestInit) => unknown) {
  const spy = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = typeof input === "string" ? input : String(input);
    const body = handler(url, init);
    if (body === undefined) return new Response("not found", { status: 404 });
    return new Response(JSON.stringify(body), {
      status: 200,
      headers: { "content-type": "application/json" },
    });
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

const ONE = {
  id: 7,
  event: "site.deploy.failed",
  title: "Deploy failed",
  body: "example.tnhc.dev could not build",
  href: "/hosting",
  readAt: null,
  createdAt: "2026-08-21T10:00:00.000Z",
};

function renderBell() {
  return render(
    <MemoryRouter>
      <NotificationBell />
    </MemoryRouter>,
  );
}

beforeEach(() => vi.useRealTimers());
afterEach(() => vi.unstubAllGlobals());

describe("NotificationBell", () => {
  it("shows the unread count from the server", async () => {
    stubFetch((url) => (url.includes("unread-count") ? { unread: 3 } : { notifications: [] }));
    renderBell();
    expect(await screen.findByText("3")).toBeTruthy();
  });

  it("names the count for a screen reader, not just a bare number", async () => {
    stubFetch((url) => (url.includes("unread-count") ? { unread: 2 } : { notifications: [] }));
    renderBell();
    // A badge reading "2" tells a sighted user everything and a screen-reader
    // user nothing. The accessible name has to carry the meaning.
    const button = await screen.findByRole("button", { name: /2 unread/i });
    expect(button).toBeTruthy();
  });

  it("shows no badge when there is nothing unread", async () => {
    stubFetch((url) => (url.includes("unread-count") ? { unread: 0 } : { notifications: [] }));
    renderBell();
    await waitFor(() => expect(screen.getByRole("button", { name: /notifications/i })).toBeTruthy());
    expect(screen.queryByText("0")).toBeNull();
  });

  it("loads the list only when opened", async () => {
    const spy = stubFetch((url) =>
      url.includes("unread-count") ? { unread: 1 } : { notifications: [ONE] },
    );
    renderBell();
    await screen.findByText("1");
    // Opening is what costs a list fetch; rendering the header must not.
    expect(spy.mock.calls.some(([u]) => String(u).endsWith("/api/notifications"))).toBe(false);

    fireEvent.click(screen.getByRole("button", { name: /unread/i }));
    expect(await screen.findByText("Deploy failed")).toBeTruthy();
  });

  it("marks one read and drops the count", async () => {
    let unread = 1;
    const seen: string[] = [];
    stubFetch((url, init) => {
      seen.push(`${init?.method ?? "GET"} ${new URL(url, "http://x").pathname}`);
      if (url.includes("unread-count")) return { unread };
      if (url.endsWith("/7/read")) {
        unread = 0;
        return { ok: true };
      }
      return { notifications: [ONE] };
    });
    renderBell();
    fireEvent.click(await screen.findByRole("button", { name: /unread/i }));
    fireEvent.click(await screen.findByText("Deploy failed"));
    await waitFor(() => expect(seen).toContain("POST /api/notifications/7/read"));
  });

  it("marks all read", async () => {
    // Asserts the POST, not just the badge. The badge clears optimistically,
    // so an assertion on the badge alone passes with the request deleted —
    // verified by removing it and watching this test stay green.
    let unread = 4;
    const seen: string[] = [];
    stubFetch((url, init) => {
      seen.push(`${init?.method ?? "GET"} ${new URL(url, "http://x").pathname}`);
      if (url.includes("unread-count")) return { unread };
      if (url.includes("read-all")) {
        unread = 0;
        return { marked: 4 };
      }
      return { notifications: [ONE] };
    });
    renderBell();
    fireEvent.click(await screen.findByRole("button", { name: /unread/i }));
    fireEvent.click(await screen.findByRole("button", { name: /mark all read/i }));
    await waitFor(() => expect(seen).toContain("POST /api/notifications/read-all"));
    expect(screen.queryByText("4")).toBeNull();
  });

  it("stays silent when the notifications service is down", async () => {
    // Hosting being unreachable is not the user's problem to solve from the
    // header of an unrelated app. A red error in the chrome of every page
    // would be worse than no bell at all.
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response("upstream unavailable", { status: 503 })),
    );
    const { container } = renderBell();
    await act(async () => { await Promise.resolve(); });
    await waitFor(() => expect(container.textContent).not.toContain("Could not"));
    expect(container.querySelector("[data-error]")).toBeNull();
  });

  it("closes on Escape", async () => {
    stubFetch((url) => (url.includes("unread-count") ? { unread: 1 } : { notifications: [ONE] }));
    renderBell();
    fireEvent.click(await screen.findByRole("button", { name: /unread/i }));
    expect(await screen.findByText("Deploy failed")).toBeTruthy();
    fireEvent.keyDown(document, { key: "Escape" });
    await waitFor(() => expect(screen.queryByText("Deploy failed")).toBeNull());
  });

  it("says so when there is nothing to show", async () => {
    stubFetch((url) => (url.includes("unread-count") ? { unread: 0 } : { notifications: [] }));
    renderBell();
    fireEvent.click(await screen.findByRole("button", { name: /notifications/i }));
    expect(await screen.findByText(/nothing yet/i)).toBeTruthy();
  });
});
