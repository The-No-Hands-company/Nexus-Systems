import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { Pill } from "../../../../../../packages/nexus-design/src/components/ui/pill";
import type { MailSummary } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

/**
 * "Count plus flagged" per the spec — not a full inbox list. The widget's job
 * is a glance at how much is waiting and what needs attention, not a second
 * copy of MailList.
 */
export default function Unread() {
  const [state, setState] = useState<WidgetState<MailSummary[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    fetch("/ipa/mail/messages?folder=inbox&unread=1")
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(String(r.status)))))
      .then((body: unknown) => {
        // Every other mail function in api.ts (listMailMessages, threadMessages,
        // searchMail) returns a bare array, not an enveloped object — this
        // endpoint may follow that convention or the enveloped one used
        // elsewhere in this file's siblings (notifications, calendar events).
        // Accept both rather than guessing wrong and crashing render; anything
        // else is a shape we don't understand, which is an error, not empty.
        const messages = Array.isArray(body)
          ? body
          : Array.isArray((body as { messages?: unknown })?.messages)
            ? (body as { messages: unknown[] }).messages
            : undefined;
        if (!messages) throw new Error("malformed_response");
        if (!cancelled) setState({ status: "ready", data: messages as MailSummary[] });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Mail unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Unread" state={state} empty="No unread mail">
      {(messages) => {
        if (messages.length === 0) return null;
        const flagged = messages.filter((m) => m.flagged);
        return (
          <div className="flex flex-col gap-3">
            <Link
              to="/mail"
              className="flex items-baseline gap-2 text-2xl font-semibold text-zinc-100 hover:text-accent"
            >
              {messages.length}
              <span className="text-sm font-normal text-zinc-500">unread</span>
            </Link>
            {flagged.length > 0 && (
              <ul className="flex flex-col gap-1">
                {flagged.map((m) => (
                  <li
                    key={m.id}
                    className="flex items-center gap-2 border-b border-zinc-700 py-1 text-sm last:border-0"
                  >
                    <Pill tone="warning">flagged</Pill>
                    <Link
                      to={`/mail/m/${m.id}`}
                      className="min-w-0 truncate text-zinc-200 hover:text-accent"
                    >
                      {m.subject ?? "(no subject)"}
                    </Link>
                  </li>
                ))}
              </ul>
            )}
          </div>
        );
      }}
    </WidgetShell>
  );
}
