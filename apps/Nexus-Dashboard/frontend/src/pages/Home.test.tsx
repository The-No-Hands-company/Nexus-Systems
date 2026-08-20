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

/** Routes /api/v1/auth/me and /api/apps independently. */
function stubFetch(signedIn: boolean) {
  vi.stubGlobal("fetch", vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (u === "/api/v1/auth/me") {
      return signedIn
        ? jsonResponse({ user: { id: "u1", username: "ada", email: "a@x.dev", role: "user" } })
        : jsonResponse({ error: "unauthenticated" }, 401);
    }
    if (u === "/api/apps") {
      return jsonResponse({
        apps: [{ id: "nexus-chat", name: "Nexus Chat", description: "Chat",
                 url: "https://chat.tnhc.dev", path: "/chat", health: "healthy" }],
      });
    }
    throw new Error(`unexpected fetch: ${u}`);
  }));
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Home", () => {
  it("shows the app grid to a signed-in user", async () => {
    stubFetch(true);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText("Nexus Chat")).toBeTruthy());
  });

  it("puts the signed-in grid inside the shell, not bare", async () => {
    // The home page used to render with no header and no sidebar, so the front
    // door looked like a different, older application than every route behind
    // it — the product appeared to start only once you clicked into an app.
    stubFetch(true);
    render(
      <MemoryRouter>
        <Home sidebar={<nav aria-label="App launcher" />} />
      </MemoryRouter>,
    );
    await waitFor(() => expect(screen.getByText("Nexus Chat")).toBeTruthy());
    expect(screen.getByRole("banner")).toBeTruthy();
    expect(screen.getByRole("navigation", { name: "App launcher" })).toBeTruthy();
  });

  it("leaves a signed-out visitor bare, even when given a sidebar", async () => {
    // Chrome advertising a launcher to somebody with no session and nothing to
    // launch is worse than no chrome at all.
    stubFetch(false);
    render(
      <MemoryRouter>
        <Home sidebar={<nav aria-label="App launcher" />} />
      </MemoryRouter>,
    );
    await waitFor(() => expect(screen.queryByRole("banner")).toBeNull());
    expect(screen.queryByRole("navigation", { name: "App launcher" })).toBeNull();
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

  it("never shows the grid to a signed-out visitor", async () => {
    stubFetch(false);
    render(<MemoryRouter><Home /></MemoryRouter>);
    await waitFor(() => expect(screen.getByRole("link", { name: /sign in/i })).toBeTruthy());
    expect(screen.queryByText("Nexus Chat")).toBeNull();
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
