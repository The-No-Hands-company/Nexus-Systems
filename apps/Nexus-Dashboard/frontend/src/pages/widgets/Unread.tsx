import { useEffect, useState } from "react";
import type { MailSummary } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

export default function Unread() {
  const [state, setState] = useState<WidgetState<MailSummary[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    fetch("/ipa/mail/messages?folder=inbox&unread=1")
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(String(r.status)))))
      .then((body: { messages: MailSummary[] }) => {
        if (!cancelled) setState({ status: "ready", data: body.messages });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Mail unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Unread" state={state} empty="No unread mail">
      {(messages) =>
        messages.length === 0 ? null : (
          <ul className="flex flex-col gap-1">
            {messages.map((m) => (
              <li key={m.id} className="flex gap-3 border-b border-zinc-700 py-1 text-sm last:border-0">
                <span className="truncate text-zinc-200">{m.from}</span>
                <span className="truncate text-zinc-500">{m.subject ?? "(no subject)"}</span>
              </li>
            ))}
          </ul>
        )
      }
    </WidgetShell>
  );
}
