import type { MailSummary } from "../../api";

export type Thread = {
  /// The message shown in the list — the most recent one, which is what a
  /// reader means by "this conversation".
  latest: MailSummary;
  count: number;
  /// Any message in the conversation being unread makes the whole row unread.
  /// Marking a thread read because its newest message was read would hide the
  /// older one somebody has not seen.
  anyUnread: boolean;
};

/**
 * Group a flat message list into conversations.
 *
 * Grouping happens here rather than in SQL because the list endpoint is
 * already scoped to one folder and one mailbox, so the set is small and the
 * server would have to invent a paging story for threads that spans folders.
 * If a mailbox ever grows past what one page can hold, this is the thing to
 * move server-side.
 */
export function groupIntoThreads(messages: MailSummary[]): Thread[] {
  const byThread = new Map<string, MailSummary[]>();

  for (const m of messages) {
    // A message with no thread id is its own conversation rather than being
    // grouped with every other unthreaded message.
    const key = m.thread_id || `single:${m.id}`;
    const existing = byThread.get(key);
    if (existing) existing.push(m);
    else byThread.set(key, [m]);
  }

  const threads: Thread[] = [];
  for (const group of byThread.values()) {
    const sorted = [...group].sort(
      (a, b) => new Date(b.received_at).getTime() - new Date(a.received_at).getTime(),
    );
    threads.push({
      latest: sorted[0],
      count: sorted.length,
      anyUnread: sorted.some((m) => !m.seen),
    });
  }

  // Newest conversation first, by its most recent message.
  return threads.sort(
    (a, b) =>
      new Date(b.latest.received_at).getTime() - new Date(a.latest.received_at).getTime(),
  );
}
