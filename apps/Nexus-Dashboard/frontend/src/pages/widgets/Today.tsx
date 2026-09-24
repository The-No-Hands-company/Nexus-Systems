import { useEffect, useState } from "react";
import WidgetShell, { type WidgetState } from "./WidgetShell";

type Event = { id: string; title: string; startTime: string };

export default function Today() {
  const [state, setState] = useState<WidgetState<Event[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    const today = new Date().toISOString().slice(0, 10);
    fetch(`/ipa/calendar/events?from=${today}&to=${today}`)
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(String(r.status)))))
      .then((body: { events?: unknown }) => {
        // A 200 with the wrong shape is not "nothing today" — it is "we could
        // not understand the answer", which is an error, not a quiet day.
        // Throwing here (rather than coercing to []) routes it through the
        // same catch below instead of letting `undefined.length` crash render.
        if (!Array.isArray(body.events)) throw new Error("malformed_response");
        if (!cancelled) setState({ status: "ready", data: body.events as Event[] });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Calendar unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Today" state={state} empty="Nothing scheduled today">
      {(events) =>
        events.length === 0 ? null : (
          <ul className="flex flex-col gap-1">
            {events.map((e) => (
              <li key={e.id} className="flex gap-3 border-b border-zinc-700 py-1 text-sm last:border-0">
                <span className="font-mono text-xs text-accent">{e.startTime.slice(11, 16)}</span>
                <span className="text-zinc-200">{e.title}</span>
              </li>
            ))}
          </ul>
        )
      }
    </WidgetShell>
  );
}
