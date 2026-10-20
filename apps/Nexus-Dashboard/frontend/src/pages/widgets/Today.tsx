import { useEffect, useState } from "react";
import { listCalendarEvents, type CalendarEvent } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

export default function Today() {
  const [state, setState] = useState<WidgetState<CalendarEvent[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    const today = new Date().toISOString().slice(0, 10);
    listCalendarEvents(today, today)
      .then((events) => {
        if (!cancelled) setState({ status: "ready", data: events });
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
