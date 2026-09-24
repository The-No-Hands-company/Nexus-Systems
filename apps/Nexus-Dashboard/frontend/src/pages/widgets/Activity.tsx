import { useEffect, useState } from "react";
import type { Notification } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

export default function Activity() {
  const [state, setState] = useState<WidgetState<Notification[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    fetch("/ipa/notifications")
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(String(r.status)))))
      .then((body: { notifications?: unknown }) => {
        // A 200 that doesn't carry a `notifications` array is not "nothing
        // recent" — it is an answer we could not understand. Throw so it
        // reaches the same error state below instead of coercing to [] or
        // reading `.length` off undefined during render.
        if (!Array.isArray(body.notifications)) throw new Error("malformed_response");
        if (!cancelled) setState({ status: "ready", data: body.notifications as Notification[] });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Notifications unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Activity" state={state} empty="Nothing recent">
      {(notifications) =>
        notifications.length === 0 ? null : (
          <ul className="flex flex-col gap-1">
            {notifications.map((n) => (
              <li key={n.id} className="flex gap-3 border-b border-zinc-700 py-1 text-sm last:border-0">
                <span className="text-zinc-200">{n.title}</span>
              </li>
            ))}
          </ul>
        )
      }
    </WidgetShell>
  );
}
