import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Home from "./Home";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

/**
 * Routes /api/v1/auth/me and /api/apps independently, and answers every other
 * fetch (the widgets' calendar/mail/notifications calls) with a rejection —
 * each widget owns its own catch, so this just exercises their error states
 * rather than the happy path, without needing to stub four more endpoints.
 */
function stubFetch(signedIn: boolean, role = "user") {
  vi.stubGlobal("fetch", vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (u === "/ipa/v1/auth/me") {
      return signedIn
        ? jsonResponse({ user: { id: "u1", username: "ada", email: "a@x.dev", role } })
        : jsonResponse({ error: "unauthenticated" }, 401);
    }
    if (u === "/ipa/apps") {
      return jsonResponse({
        apps: [{ id: "nexus-chat", name: "Nexus Chat", description: "Chat",
                 url: "https://chat.tnhc.dev", path: "/chat", health: "healthy" }],
      });
    }
    // Widgets (Today, Unread, Activity) fetch endpoints this test does not
    // care about; each has its own catch, so a rejection here just puts that
    // one widget in its error state instead of failing the render.
    throw new Error(`not stubbed: ${u}`);
  }));
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Home", () => {
  it("shows the dashboard to a signed-in user", async () => {
    stubFetch(true);
    render(<MemoryRouter><Home /></MemoryRouter>);
    // The health strip and the Pinned widget both source from the same app
    // list, so the app name legitimately appears more than once.
    await waitFor(() => expect(screen.getAllByText("Nexus Chat").length).toBeGreaterThan(0));
    expect(screen.getByRole("heading", { name: /pinned/i })).toBeTruthy();
  });

  it("puts the signed-in dashboard inside the shell, not bare", async () => {
    // The home page used to render with no header and no sidebar, so the front
    // door looked like a different, older application than every route behind
    // it — the product appeared to start only once you clicked into an app.
    stubFetch(true);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByRole("banner")).toBeTruthy());
    expect(screen.getByRole("navigation", { name: "Shell" })).toBeTruthy();
  });

  it("shows the founder the Operator link right on home", async () => {
    // /admin worked but nothing linked to it; the founder landed here and the
    // header carried no way in. Home was also the one shell that dropped the
    // user prop entirely, so there was no identity chip either.
    stubFetch(true, "founder");
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByRole("link", { name: "Operator" })).toBeTruthy());
    expect(screen.getByRole("link", { name: "Operator" }).getAttribute("href")).toBe("/admin");
  });

  it("leaves a signed-out visitor bare", async () => {
    // Chrome advertising an apps drawer to somebody with no session and
    // nothing to launch is worse than no chrome at all.
    stubFetch(false);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.queryByRole("banner")).toBeNull());
  });

  it("shows the way in to a signed-out visitor", async () => {
    stubFetch(false);
    render(<MemoryRouter><Home /></MemoryRouter>);

    // This host is public so a stranger can get here at all; it must offer
    // both signing in and requesting an account.
    await waitFor(() => expect(screen.getByRole("link", { name: /sign in/i })).toBeTruthy());
    expect(screen.getByRole("link", { name: /request access/i })).toBeTruthy();
    expect(screen.getByRole("link", { name: /claim/i })).toBeTruthy();
  });

  it("never shows the launcher to a signed-out visitor", async () => {
    stubFetch(false);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByRole("link", { name: /sign in/i })).toBeTruthy());

    // The invariant is "no launcher", not "no app names".
    //
    // This asserted queryByText("Nexus Chat") was null, which conflated the
    // two. App names are not secret: /api/apps serves them unauthenticated and
    // tnhc.dev/apps publishes the whole directory. Hiding them on this one
    // page protected nothing while leaving the front door unable to show a
    // stranger what they would be signing in to.
    //
    // What must not appear is an interactive affordance — a link into an app
    // for someone with no session, which lands them on a login redirect and
    // looks broken. The signed-out page may name apps; it may not offer to
    // open them.
    expect(screen.queryByRole("link", { name: /nexus chat/i })).toBeNull();
    expect(screen.queryByRole("link", { name: /nexus mail/i })).toBeNull();
  });

  it("sends the user back here after signing in", async () => {
    stubFetch(false);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByRole("link", { name: /sign in/i })).toBeTruthy());

    const href = screen.getByRole("link", { name: /sign in/i }).getAttribute("href") ?? "";
    expect(href).toContain("/login");
    expect(href).toContain(`redirect_uri=${encodeURIComponent(window.location.origin)}`);
  });
});
