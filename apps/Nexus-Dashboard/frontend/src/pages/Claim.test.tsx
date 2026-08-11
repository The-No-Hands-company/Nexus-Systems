import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Claim from "./Claim";

const CODES = Array.from({ length: 10 }, (_, i) => String(i).repeat(32));
const GOOD_PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret

function jsonResponse(body: unknown, status: number) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function fill(code: string, password = GOOD_PASSWORD) {
  fireEvent.change(screen.getByLabelText(/email/i), { target: { value: "s@x.dev" } });
  fireEvent.change(screen.getByLabelText(/claim code/i), { target: { value: code } });
  fireEvent.change(screen.getByLabelText(/password/i), { target: { value: password } });
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Claim", () => {
  it("shows all ten recovery codes and blocks continuing until they are confirmed saved", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse(
      { user: { id: "usr-1", status: "active" }, recoveryCodes: CODES }, 200,
    )));

    render(<Claim />);
    fill("a".repeat(32));
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(CODES[0]!)).toBeTruthy());
    for (const c of CODES) expect(screen.getByText(c)).toBeTruthy();

    // Losing these means losing the account permanently — the UI must not let
    // the user click past them by reflex.
    const cont = screen.getByRole("button", { name: /continue/i }) as HTMLButtonElement;
    expect(cont.disabled).toBe(true);
    fireEvent.click(screen.getByRole("checkbox"));
    expect(cont.disabled).toBe(false);
  });

  it("warns that a lost account cannot be recovered by anyone", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse(
      { user: { id: "usr-1" }, recoveryCodes: CODES }, 200,
    )));

    render(<Claim />);
    fill("a".repeat(32));
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(/cannot be recovered|permanently|no way to restore/i)).toBeTruthy());
  });

  it("reports a rejected claim code", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ error: "invalid_code" }, 400)));

    render(<Claim />);
    fill("0".repeat(32));
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(/invalid|not recognised|incorrect/i)).toBeTruthy());
  });

  it("explains a not-yet-approved account rather than showing a raw code", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ error: "not_approved" }, 400)));

    render(<Claim />);
    fill("a".repeat(32));
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(/not yet approved|still pending|waiting/i)).toBeTruthy());
  });

  it("explains a weak password in terms of the actual rule", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ error: "weak_password" }, 400)));

    render(<Claim />);
    fill("a".repeat(32), "short");  // pragma: allowlist secret
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(/12/)).toBeTruthy());
  });

  it("posts to the same-origin auth path", async () => {
    const spy = vi.fn(async (_url: RequestInfo | URL, _init?: RequestInit) => jsonResponse({ user: { id: "u" }, recoveryCodes: CODES }, 200));
    vi.stubGlobal("fetch", spy);

    render(<Claim />);
    fill("a".repeat(32));
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(spy).toHaveBeenCalled());
    expect(String(spy.mock.calls[0]![0])).toBe("/api/v1/auth/claim");
  });
});
