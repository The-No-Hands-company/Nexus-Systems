import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import RequestAccess from "./RequestAccess";

function jsonResponse(body: unknown, status: number) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function fill(username: string, email: string) {
  fireEvent.change(screen.getByLabelText(/username/i), { target: { value: username } });
  fireEvent.change(screen.getByLabelText(/email/i), { target: { value: email } });
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("RequestAccess", () => {
  it("shows the claim code once, with a warning to save it", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse(
      { user: { id: "usr-1", status: "pending" }, claimCode: "a".repeat(32) }, 201,
    )));

    render(<RequestAccess />);
    fill("sam", "s@x.dev");
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(screen.getByText("a".repeat(32))).toBeTruthy());
    // The code cannot be retrieved again, so the UI must say so plainly.
    expect(screen.getByText(/only time|save it|cannot be shown again/i)).toBeTruthy();
  });

  it("surfaces a duplicate-username error instead of failing silently", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse(
      { error: "Username 'sam' already exists" }, 409,
    )));

    render(<RequestAccess />);
    fill("sam", "s@x.dev");
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(screen.getByText(/already exists/i)).toBeTruthy());
  });

  it("reports a network failure rather than appearing to hang", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("network down"); }));

    render(<RequestAccess />);
    fill("sam", "s@x.dev");
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(screen.getByText(/could not reach|try again|failed/i)).toBeTruthy());
  });

  it("posts to the same-origin auth path", async () => {
    const spy = vi.fn(async (_url: RequestInfo | URL, _init?: RequestInit) => jsonResponse(
      { user: { id: "u" }, claimCode: "b".repeat(32) }, 201,
    ));
    vi.stubGlobal("fetch", spy);

    render(<RequestAccess />);
    fill("sam", "s@x.dev");
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(spy).toHaveBeenCalled());
    // Relative, not https://auth.<domain>/... — the dashboard proxies it, and
    // a cross-origin call here would need credentialed CORS.
    expect(String(spy.mock.calls[0]![0])).toBe("/ipa/v1/auth/access-requests");
  });

  it("does not submit without a username and email", () => {
    const spy = vi.fn();
    vi.stubGlobal("fetch", spy);

    render(<RequestAccess />);
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));
    expect(spy).not.toHaveBeenCalled();
  });
});
