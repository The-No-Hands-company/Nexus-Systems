import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Admin from "./Admin";

const ADMIN = { id: "u-admin", username: "boss", email: "boss@x.dev", role: "founder" };
const PLAIN = { id: "u1", username: "ada", email: "ada@x.dev", role: "user" };

const REQUESTS = [
  { id: "usr-1", username: "sam", email: "s@x.dev", status: "pending", note: "want to try it",
    createdAt: "2026-08-11T10:00:00Z" },
  { id: "usr-2", username: "tia", email: "t@x.dev", status: "pending",
    createdAt: "2026-08-11T11:00:00Z" },
];

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function stubFetch(who: typeof ADMIN | typeof PLAIN, overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL, init?: RequestInit) => {
    const u = String(url);
    const key = `${init?.method ?? "GET"} ${u}`;
    if (overrides[key]) return overrides[key]!();
    if (u === "/api/v1/auth/me") return jsonResponse({ user: who });
    if (u === "/api/v1/auth/access-requests") return jsonResponse({ requests: REQUESTS });
    if (u.endsWith("/approve") || u.endsWith("/reject")) return jsonResponse({ user: {} });
    if (u === "/api/v1/auth/invites") {
      return jsonResponse({ id: "inv-1", code: "c".repeat(32), expiresAt: "2026-09-10T00:00:00Z" }, 201);
    }
    throw new Error(`unexpected fetch: ${key}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Admin", () => {
  it("lists pending requests with the note the person wrote", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());
    expect(screen.getByText("tia")).toBeTruthy();
    expect(screen.getByText(/want to try it/)).toBeTruthy();
  });

  it("removes a request from the queue once approved", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.queryByText("sam")).toBeNull());
    expect(screen.getByText("tia")).toBeTruthy();
  });

  it("removes a request from the queue once rejected", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /reject/i })[0]!);
    await waitFor(() => expect(screen.queryByText("sam")).toBeNull());
  });

  it("keeps the request in the queue when approval fails", async () => {
    // Dropping it optimistically would tell the operator someone was approved
    // when they were not, and the person would never be able to claim.
    stubFetch(ADMIN, {
      "POST /api/v1/auth/access-requests/usr-1/approve": () =>
        jsonResponse({ error: "not_pending" }, 409),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText("sam")).toBeTruthy());

    fireEvent.click(screen.getAllByRole("button", { name: /approve/i })[0]!);
    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText("sam")).toBeTruthy();
  });

  it("mints an invite and shows the code once", async () => {
    stubFetch(ADMIN);
    render(<Admin />);
    await waitFor(() => expect(screen.getByRole("button", { name: /create invite/i })).toBeTruthy());

    fireEvent.click(screen.getByRole("button", { name: /create invite/i }));
    await waitFor(() => expect(screen.getByText("c".repeat(32))).toBeTruthy());
    expect(screen.getByText(/only time|save|shown once|cannot be shown again/i)).toBeTruthy();
  });

  it("renders nothing for a non-admin", async () => {
    // Presentation only — the endpoints are guarded by users:approve
    // server-side, which is what actually enforces this.
    stubFetch(PLAIN);
    const { container } = render(<Admin />);
    await waitFor(() => expect(container.textContent).not.toContain("Loading"));
    expect(screen.queryByText("sam")).toBeNull();
    expect(screen.queryByRole("button", { name: /create invite/i })).toBeNull();
  });

  it("says so when the queue is empty rather than looking broken", async () => {
    stubFetch(ADMIN, {
      "GET /api/v1/auth/access-requests": () => jsonResponse({ requests: [] }),
    });
    render(<Admin />);
    await waitFor(() => expect(screen.getByText(/no pending|nothing waiting|queue is empty/i)).toBeTruthy());
  });
});
