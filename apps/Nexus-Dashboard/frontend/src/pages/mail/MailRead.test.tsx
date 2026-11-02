import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { afterEach, describe, expect, it, vi } from "vitest";
import MailRead from "./MailRead";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function message(folder_kind: string, folder_name: string) {
  return {
    id: "m-1",
    thread_id: null,
    subject: "Design review",
    from: "Alice <alice@tnhc.dev>",
    to: "alice@tnhc.dev",
    date: "2026-01-01",
    text: "Please review this.",
    html: null,
    blocked_remote: false,
    attachments: [],
    folder_kind,
    folder_name,
  };
}

function stubFetch(folderKind: string, folderName: string) {
  const spy = vi.fn(async (url: RequestInfo | URL, init?: RequestInit) => {
    const path = String(url);
    if (path.endsWith("/ipa/mail/messages/m-1") && init?.method === "DELETE") {
      return new Response(null, { status: 204 });
    }
    if (path.endsWith("/ipa/mail/messages/m-1")) return jsonResponse(message(folderKind, folderName));
    if (path.endsWith("/ipa/mail/messages/m-1/seen")) return new Response(null, { status: 204 });
    if (path.endsWith("/ipa/mail/messages/m-1/folder")) return new Response(null, { status: 204 });
    throw new Error(`unexpected fetch: ${path}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

afterEach(() => {
  vi.restoreAllMocks();
});

describe("MailRead", () => {
  it("moves inbox messages to Trash and marks them as Spam", async () => {
    const spy = stubFetch("inbox", "INBOX");
    render(
      <MemoryRouter initialEntries={["/mail/m/m-1"]}>
        <Routes><Route path="/mail/m/:messageId" element={<MailRead />} /></Routes>
      </MemoryRouter>,
    );

    await screen.findByText("Design review");
    fireEvent.click(screen.getByRole("button", { name: "Move to Trash" }));
    await waitFor(() => expect(spy).toHaveBeenCalledWith(
      expect.stringContaining("/ipa/mail/messages/m-1/folder"),
      expect.objectContaining({ method: "POST", body: JSON.stringify({ folder: "trash" }) }),
    ));

    fireEvent.click(screen.getByRole("button", { name: "Mark as Spam" }));
    await waitFor(() => expect(spy).toHaveBeenCalledWith(
      expect.stringContaining("/ipa/mail/messages/m-1/folder"),
      expect.objectContaining({ method: "POST", body: JSON.stringify({ folder: "junk" }) }),
    ));
  });

  it("offers Not Spam for Junk and restore for Trash", async () => {
    const junkSpy = stubFetch("custom", "Junk");
    const { unmount } = render(
      <MemoryRouter initialEntries={["/mail/m/m-1"]}>
        <Routes><Route path="/mail/m/:messageId" element={<MailRead />} /></Routes>
      </MemoryRouter>,
    );

    await screen.findByText("Design review");
    fireEvent.click(screen.getByRole("button", { name: "Not Spam" }));
    await waitFor(() => expect(junkSpy).toHaveBeenCalledWith(
      expect.stringContaining("/ipa/mail/messages/m-1/folder"),
      expect.objectContaining({ body: JSON.stringify({ folder: "inbox" }) }),
    ));
    unmount();

    const trashSpy = stubFetch("trash", "Trash");
    render(
      <MemoryRouter initialEntries={["/mail/m/m-1"]}>
        <Routes><Route path="/mail/m/:messageId" element={<MailRead />} /></Routes>
      </MemoryRouter>,
    );

    await screen.findByText("Design review");
    fireEvent.click(screen.getByRole("button", { name: "Restore to Inbox" }));
    await waitFor(() => expect(trashSpy).toHaveBeenCalledWith(
      expect.stringContaining("/ipa/mail/messages/m-1/folder"),
      expect.objectContaining({ body: JSON.stringify({ folder: "inbox" }) }),
    ));
  });

  it("confirms and permanently deletes from Trash", async () => {
    const spy = stubFetch("trash", "Trash");
    render(
      <MemoryRouter initialEntries={["/mail/m/m-1"]}>
        <Routes><Route path="/mail/m/:messageId" element={<MailRead />} /></Routes>
      </MemoryRouter>,
    );

    await screen.findByText("Design review");
    fireEvent.click(screen.getByRole("button", { name: "Delete permanently" }));
    fireEvent.click(screen.getByRole("button", { name: "Confirm permanent delete" }));
    await waitFor(() => expect(spy).toHaveBeenCalledWith(
      expect.stringContaining("/ipa/mail/messages/m-1"),
      expect.objectContaining({ method: "DELETE" }),
    ));
    await waitFor(() => expect(screen.getByRole("status").textContent).toMatch(/deleted permanently/i));
  });
});
