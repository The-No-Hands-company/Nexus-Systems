import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Account from "./Account";

const ME = { id: "u1", username: "ada", email: "ada@x.dev", role: "user" };
const SESSIONS = [
  { id: "sess-1", ipAddress: "1.2.3.4", userAgent: "Firefox", createdAt: "2026-08-11T10:00:00Z" },
  { id: "sess-2", ipAddress: "5.6.7.8", userAgent: "Chrome", createdAt: "2026-08-11T11:00:00Z" },
];
const NEW_CODES = Array.from({ length: 10 }, (_, i) => `n${i}`.repeat(16));

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

/** Routes each endpoint the page uses; `overrides` replaces one of them. */
function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL, init?: RequestInit) => {
    const u = String(url);
    const key = `${init?.method ?? "GET"} ${u}`;
    if (overrides[key]) return overrides[key]!();
    if (u === "/api/v1/auth/me") return jsonResponse({ user: ME });
    if (u === "/api/v1/auth/sessions") return jsonResponse({ sessions: SESSIONS });
    if (u === "/api/v1/auth/recovery-codes") return jsonResponse({ remaining: 7 });
    if (u === "/api/v1/auth/recovery-codes/regenerate") return jsonResponse({ recoveryCodes: NEW_CODES });
    if (u.endsWith("/password")) return jsonResponse({ success: true });
    if (u.endsWith("/revoke")) return jsonResponse({ success: true });
    throw new Error(`unexpected fetch: ${key}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Account", () => {
  it("shows who you are signed in as", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByText("ada")).toBeTruthy());
    expect(screen.getByText("ada@x.dev")).toBeTruthy();
  });

  it("shows how many recovery codes remain", async () => {
    stubFetch();
    render(<Account />);
    // Specific: a bare /7/ also matches the 5.6.7.8 session IP.
    await waitFor(() => expect(screen.getByText(/7 unused/)).toBeTruthy());
  });

  it("regenerates codes and shows the new set behind a save confirmation", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByRole("button", { name: /regenerate/i })).toBeTruthy());
    fireEvent.click(screen.getByRole("button", { name: /regenerate/i }));

    await waitFor(() => expect(screen.getByText(NEW_CODES[0]!)).toBeTruthy());
    // Same gate as first claim: these replace the old set and are shown once.
    const done = screen.getByRole("button", { name: /done/i }) as HTMLButtonElement;
    expect(done.disabled).toBe(true);
    fireEvent.click(screen.getByRole("checkbox"));
    expect(done.disabled).toBe(false);
  });

  it("warns that regenerating retires the previous codes", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByRole("button", { name: /regenerate/i })).toBeTruthy());
    expect(screen.getByText(/replace|invalidate|stop working|no longer/i)).toBeTruthy();
  });

  it("lists active sessions and removes one when revoked", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByText(/1\.2\.3\.4/)).toBeTruthy());
    expect(screen.getByText(/5\.6\.7\.8/)).toBeTruthy();

    fireEvent.click(screen.getAllByRole("button", { name: /revoke/i })[0]!);
    await waitFor(() => expect(screen.queryByText(/1\.2\.3\.4/)).toBeNull());
    expect(screen.getByText(/5\.6\.7\.8/)).toBeTruthy();
  });

  it("surfaces the server's reason when a password change is refused", async () => {
    stubFetch({
      "POST /api/v1/auth/users/u1/password": () => jsonResponse({ error: "weak_password" }, 400),
    });
    render(<Account />);
    await waitFor(() => expect(screen.getByLabelText(/current password/i)).toBeTruthy());

    fireEvent.change(screen.getByLabelText(/current password/i), { target: { value: "old-password-1234" } });
    fireEvent.change(screen.getByLabelText(/new password/i), { target: { value: "short" } });  // pragma: allowlist secret
    fireEvent.click(screen.getByRole("button", { name: /change password/i }));

    await waitFor(() => expect(screen.getByText(/12/)).toBeTruthy());
  });

  it("confirms a successful password change", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByLabelText(/current password/i)).toBeTruthy());

    fireEvent.change(screen.getByLabelText(/current password/i), { target: { value: "old-password-1234" } });
    fireEvent.change(screen.getByLabelText(/new password/i), { target: { value: "a-much-better-password" } });
    fireEvent.click(screen.getByRole("button", { name: /change password/i }));

    await waitFor(() => expect(screen.getByText(/changed|updated/i)).toBeTruthy());
  });

  it("posts the password change to the caller's own id", async () => {
    const spy = stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByLabelText(/current password/i)).toBeTruthy());

    fireEvent.change(screen.getByLabelText(/current password/i), { target: { value: "old-password-1234" } });
    fireEvent.change(screen.getByLabelText(/new password/i), { target: { value: "a-much-better-password" } });
    fireEvent.click(screen.getByRole("button", { name: /change password/i }));

    await waitFor(() => {
      const called = spy.mock.calls.some(([u]) => String(u) === "/api/v1/auth/users/u1/password");
      expect(called).toBe(true);
    });
  });
});
