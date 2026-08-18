import { describe, it, expect } from "vitest";
import { groupIntoThreads } from "./threads";
import type { MailSummary } from "../../api";

function msg(over: Partial<MailSummary> & { id: string }): MailSummary {
  return {
    thread_id: "t-1",
    subject: "Design review",
    from: "alice@tnhc.dev",
    received_at: "2026-08-18T10:00:00Z",
    seen: true,
    flagged: false,
    snippet: null,
    ...over,
  };
}

describe("groupIntoThreads", () => {
  it("collapses a conversation into one row showing its latest message", () => {
    const threads = groupIntoThreads([
      msg({ id: "a", received_at: "2026-08-18T10:00:00Z" }),
      msg({ id: "b", received_at: "2026-08-18T12:00:00Z", from: "bob@tnhc.dev" }),
    ]);

    expect(threads).toHaveLength(1);
    expect(threads[0].count).toBe(2);
    // The newest message is the one a reader means by "this conversation".
    expect(threads[0].latest.id).toBe("b");
  });

  it("marks a conversation unread when any message in it is unread", () => {
    // Going by the newest message alone would hide an older one nobody has
    // read, which is the whole reason this is a separate field.
    const threads = groupIntoThreads([
      msg({ id: "old", seen: false, received_at: "2026-08-18T09:00:00Z" }),
      msg({ id: "new", seen: true, received_at: "2026-08-18T15:00:00Z" }),
    ]);
    expect(threads[0].anyUnread).toBe(true);
  });

  it("keeps separate conversations separate", () => {
    const threads = groupIntoThreads([
      msg({ id: "a", thread_id: "t-1" }),
      msg({ id: "b", thread_id: "t-2" }),
    ]);
    expect(threads).toHaveLength(2);
  });

  it("does not lump every unthreaded message together", () => {
    // A null thread id means "unknown", not "the same conversation as every
    // other unknown" — collapsing them would hide real mail.
    const threads = groupIntoThreads([
      msg({ id: "a", thread_id: null as unknown as string }),
      msg({ id: "b", thread_id: null as unknown as string }),
    ]);
    expect(threads).toHaveLength(2);
  });

  it("orders conversations by their most recent message", () => {
    const threads = groupIntoThreads([
      msg({ id: "old", thread_id: "t-old", received_at: "2026-08-10T10:00:00Z" }),
      msg({ id: "new", thread_id: "t-new", received_at: "2026-08-18T10:00:00Z" }),
    ]);
    expect(threads.map((t) => t.latest.id)).toEqual(["new", "old"]);
  });

  it("returns nothing for an empty mailbox", () => {
    expect(groupIntoThreads([])).toEqual([]);
  });
});
