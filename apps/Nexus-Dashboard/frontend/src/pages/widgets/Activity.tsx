import { useEffect, useState } from "react";
import { listNotifications, markNotificationRead, type Notification } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

function relativeTime(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  const mins = Math.floor(diff / 60_000);
  if (mins < 1) return "just now";
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  const days = Math.floor(hrs / 24);
  return `${days}d ago`;
}

export default function Activity() {
  const [state, setState] = useState<WidgetState<Notification[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    listNotifications()
      .then((n) => { if (!cancelled) setState({ status: "ready", data: n.slice(0, 10) }); })
      .catch(() => { if (!cancelled) setState({ status: "error", message: "Notifications unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  async function markRead(id: number) {
    setState((prev) => {
      if (prev.status !== "ready") return prev;
      return {
        status: "ready",
        data: prev.data.map((n) =>
          n.id === id ? { ...n, readAt: new Date().toISOString() } : n,
        ),
      };
    });
    try { await markNotificationRead(id); } catch { /* soft fail */ }
  }

  return (
    <WidgetShell title="Activity" state={state} empty="Nothing recent">
      {(notifications) =>
        notifications.length === 0 ? null : (
          <ul className="flex flex-col divide-y divide-zinc-700/50">
            {notifications.map((n) => {
              const unread = !n.readAt;
              const inner = (
                <div className="flex items-start gap-2 py-2">
                  <span
                    aria-hidden
                    className={`mt-1.5 h-1.5 w-1.5 shrink-0 rounded-full ${unread ? "bg-accent" : "bg-transparent"}`}
                  />
                  <div className="min-w-0 flex-1">
                    <div className="flex items-baseline justify-between gap-2">
                      <span className={`text-sm leading-snug ${unread ? "font-medium text-zinc-100" : "text-zinc-300"}`}>
                        {n.title}
                      </span>
                      <span className="shrink-0 text-[11px] text-zinc-500">{relativeTime(n.createdAt)}</span>
                    </div>
                    {n.body && (
                      <p className="mt-0.5 line-clamp-1 text-xs text-zinc-500">{n.body}</p>
                    )}
                  </div>
                </div>
              );

              return (
                <li key={n.id}>
                  {n.href ? (
                    <a
                      href={n.href}
                      onClick={() => unread && void markRead(n.id)}
                      className="block hover:bg-zinc-700/30 -mx-1 px-1 rounded transition-colors"
                    >
                      {inner}
                    </a>
                  ) : (
                    <div
                      role={unread ? "button" : undefined}
                      tabIndex={unread ? 0 : undefined}
                      onClick={() => unread && void markRead(n.id)}
                      onKeyDown={(e) => e.key === "Enter" && unread && void markRead(n.id)}
                      className={unread ? "cursor-pointer -mx-1 px-1 rounded hover:bg-zinc-700/30 transition-colors" : ""}
                    >
                      {inner}
                    </div>
                  )}
                </li>
              );
            })}
          </ul>
        )
      }
    </WidgetShell>
  );
}
