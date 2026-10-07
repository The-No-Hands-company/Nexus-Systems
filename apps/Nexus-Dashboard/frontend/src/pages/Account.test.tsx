import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Account from "./Account";

const ME = { id: "u1", username: "ada", email: "ada@x.dev", role: "user" };
const SESSIONS = [
  { id: "s1", deviceId: "dev_abcdefghxyz", createdAt: "2026-10-07T10:00:00Z", expiresAt: "2026-10-08T10:00:00Z", current: true },
  { id: "s2", deviceId: "dev_zzzyyyyyqqq", createdAt: "2026-10-06T10:00:00Z", expiresAt: "2026-10-08T10:00:00Z", current: false },
];
const ACTIVITY = {
  events: [
    { event: "login_success", deviceId: "dev_abcdefghxyz", at: "2026-10-07T10:00:00Z" },
    { event: "login_failure", deviceId: null, at: "2026-10-07T09:00:00Z" },
  ],
};
const NEW_CODES = Array.from({ length: 10 }, (_, i) => `n${i}`.repeat(16));

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

/** Routes each endpoint the page uses; `overrides` replaces one of them. */
let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL, init?: RequestInit) => {
    const u = String(url);
    const key = `${init?.method ?? "GET"} ${u}`;
    if (overrides[key]) return overrides[key]!();
    if (u === "/ipa/v1/auth/me") return jsonResponse({ user: ME });
    if (u === "/ipa/v1/auth/sessions") return jsonResponse({ sessions: SESSIONS });
    if (u === "/ipa/v1/auth/activity") return jsonResponse(ACTIVITY);
    if (u === "/ipa/v1/auth/recovery-codes") return jsonResponse({ remaining: 7 });
    if (u === "/ipa/v1/auth/recovery-codes/regenerate") return jsonResponse({ recoveryCodes: NEW_CODES });
    if (u.endsWith("/password")) return jsonResponse({ success: true });
    if (u.endsWith("/revoke")) return jsonResponse({ success: true });
    throw new Error(`unexpected fetch: ${key}`);
  });
  fetchSpy = spy;
  globalThis.fetch = spy as any;
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

afterEach(() => {
  if (fetchSpy) {
    delete (globalThis as any).fetch;
    fetchSpy = null;
  }
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

  it("shows recent activity and device sessions without any address", async () => {
    stubFetch();
    const { container } = render(<Account />);
    await waitFor(() => expect(screen.getByRole("heading", { name: "Your recent activity" })).toBeTruthy());
    await waitFor(() => expect(screen.getByText("Signed in")).toBeTruthy());
    expect(screen.getByText("Wrong password")).toBeTruthy();
    expect(screen.getAllByText(/Device abcdefgh/).length).toBeGreaterThan(0);
    expect(container.textContent).not.toMatch(/\d+\.\d+\.\d+\.\d+/);
    expect(container.textContent).not.toMatch(/null/);
    expect(screen.getByText("This device")).toBeTruthy();
    expect(screen.getAllByRole("button", { name: "Sign this device out" }).length).toBe(1);
  });

  it("signs another device out and removes its row", async () => {
    stubFetch();
    render(<Account />);
    await waitFor(() => expect(screen.getByRole("button", { name: "Sign this device out" })).toBeTruthy());
    expect(screen.getByText("Device zzzyyyyy")).toBeTruthy();
    fireEvent.click(screen.getByRole("button", { name: "Sign this device out" }));
    await waitFor(() => expect(screen.queryByText("Device zzzyyyyy")).toBeNull());
  });

  it("surfaces the server's reason when a password change is refused", async () => {
    stubFetch({
      "POST /ipa/v1/auth/users/u1/password": () => jsonResponse({ error: "weak_password" }, 400),
    });
    render(<Account />);
    await waitFor(() => expect(screen.getByLabelText(/current password/i)).toBeTruthy());

    fireEvent.change(screen.getByLabelText(/current password/i), { target: { value: "old-password-1234" } });
    fireEvent.change(screen.getByLabelText(/new password/i), { target: { value: "short" } });  // pragma: allowlist secret
    fireEvent.click(screen.getByRole("button", { name: /change password/i }));

    await waitFor(() => expect(screen.getByText(/at least 12 characters\./)).toBeTruthy());
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
      const called = spy.mock.calls.some(([u]) => String(u) === "/ipa/v1/auth/users/u1/password");
      expect(called).toBe(true);
    });
  });
});
