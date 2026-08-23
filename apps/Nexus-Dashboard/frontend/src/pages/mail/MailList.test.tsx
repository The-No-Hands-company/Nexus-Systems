import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import MailList from "./MailList";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

const FOLDERS = [
  { id: "f-inbox", name: "INBOX", kind: "inbox" },
  { id: "f-sent", name: "Sent", kind: "sent" },
];

const MESSAGES = [
  {
    id: "m-1",
    thread_id: "t-1",
    subject: "Design review",
    from: "Alice <alice@tnhc.dev>",
    received_at: new Date().toISOString(),
    seen: false,
    flagged: false,
    snippet: "thoughts on the plan",
  },
];

let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    for (const [key, fn] of Object.entries(overrides)) {
      if (u.startsWith(key)) return fn();
    }
    if (u.startsWith("/ipa/mail/folders/")) return jsonResponse(MESSAGES);
    if (u.startsWith("/ipa/mail/folders")) return jsonResponse(FOLDERS);
    if (u.startsWith("/ipa/mail/search")) return jsonResponse([]);
    throw new Error(`unexpected fetch: ${u}`);
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

describe("MailList", () => {
  it("shows the folders and the messages in them", async () => {
    stubFetch();
    render(<MemoryRouter><MailList /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("Design review")).toBeTruthy());
    expect(screen.getByText("INBOX")).toBeTruthy();
    // The display name is shown, not the raw header.
    expect(screen.getByText("Alice")).toBeTruthy();
  });

  it("defaults to the inbox when no folder is in the URL", async () => {
    const spy = stubFetch();
    render(<MemoryRouter><MailList /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("Design review")).toBeTruthy());
    // Landing nowhere would show an empty page on a mailbox full of mail.
    expect(spy.mock.calls.some(([u]) => String(u).includes("/ipa/mail/folders/f-inbox/messages"))).toBe(true);
  });

  it("says an account has no mailbox rather than calling it a failure", async () => {
    // An ordinary state for a new account. Telling someone their mail is broken
    // when they simply have no address yet is worse than saying nothing.
    stubFetch({
      "/ipa/mail/folders": () => jsonResponse({ error: "no mailbox for this account" }, 404),
    });
    render(<MemoryRouter><MailList /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/does not have a mailbox yet/i)).toBeTruthy());
    expect(screen.queryByRole("alert")).toBeNull();
  });

  it("reports mail being down as unavailable, not as an empty inbox", async () => {
    // An empty list and a broken service look identical to a user unless the
    // page says which it is.
    stubFetch({
      "/ipa/mail/folders": () => jsonResponse({ error: "mail_unavailable" }, 503),
    });
    render(<MemoryRouter><MailList /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
  });
});
