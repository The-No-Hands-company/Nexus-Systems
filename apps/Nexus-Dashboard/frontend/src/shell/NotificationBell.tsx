import { useCallback, useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import {
  listNotifications,
  unreadNotificationCount,
  markNotificationRead,
  markAllNotificationsRead,
  type Notification,
} from "../api";

/** How often to re-ask for the unread count while the shell is open. */
const POLL_MS = 60_000;

/**
 * The bell in the shell header.
 *
 * Nexus-Hosting already fanned every event out to webhooks; this is the same
 * events delivered to a person instead of a URL. It owns its own fetching
 * rather than taking data from Shell, which is deliberately layout-only.
 *
 * Every failure here is silent by design. Notifications are ancillary to
 * whatever the user came to do, and Hosting being unreachable is not a problem
 * they can act on from the header of an unrelated app — a red error in the
 * chrome of every page would be worse than no bell at all. The console still
 * gets nothing: a failed poll is expected during a Hosting deploy.
 */
export default function NotificationBell() {
  const [unread, setUnread] = useState(0);
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<Notification[] | null>(null);
  const [filter, setFilter] = useState<"all" | "deploy" | "mail" | "system">("all");
  const navigate = useNavigate();
  const rootRef = useRef<HTMLDivElement | null>(null);

  const refreshCount = useCallback(() => {
    unreadNotificationCount()
      .then(setUnread)
      .catch(() => {
        /* silent: see the component comment */
      });
  }, []);

  useEffect(() => {
    refreshCount();
    const t = setInterval(refreshCount, POLL_MS);
    return () => clearInterval(t);
  }, [refreshCount]);

  // The list is fetched on open, not on mount. Most page loads never open the
  // panel, and a list fetch per navigation would be pure load on Hosting.
  useEffect(() => {
    if (!open) return;
    let cancelled = false;
    listNotifications()
      .then((n) => { if (!cancelled) setItems(n); })
      .catch(() => { if (!cancelled) setItems([]); });
    return () => { cancelled = true; };
  }, [open]);

  // Escape and outside-click both close. A panel that can only be dismissed by
  // the button that opened it traps keyboard users.
  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => { if (e.key === "Escape") setOpen(false); };
    const onDown = (e: MouseEvent) => {
      if (rootRef.current && !rootRef.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("keydown", onKey);
    document.addEventListener("mousedown", onDown);
    return () => {
      document.removeEventListener("keydown", onKey);
      document.removeEventListener("mousedown", onDown);
    };
  }, [open]);

  function openOne(n: Notification) {
    if (!n.readAt) {
      // Optimistic: the row greys out immediately. If the POST fails the count
      // is corrected by the next poll, which is a better trade than making
      // someone wait on a round trip to dismiss a notice they have just read.
      setItems((cur) => cur?.map((x) => (x.id === n.id ? { ...x, readAt: new Date().toISOString() } : x)) ?? cur);
      setUnread((u) => Math.max(0, u - 1));
      markNotificationRead(n.id).catch(() => refreshCount());
    }
    if (n.href) {
      setOpen(false);
      if (/^https?:/i.test(n.href)) window.location.assign(n.href);
      else navigate(n.href);
    }
  }

  function readAll() {
    setItems((cur) => cur?.map((x) => ({ ...x, readAt: x.readAt ?? new Date().toISOString() })) ?? cur);
    setUnread(0);
    markAllNotificationsRead().catch(() => refreshCount());
  }

  const label = unread > 0 ? `Notifications, ${unread} unread` : "Notifications";

  const displayedItems = (items ?? []).filter((item) => {
    if (filter === "all") return true;
    const ev = item.event.toLowerCase();
    if (filter === "deploy") return ev.includes("deploy") || ev.includes("site") || ev.includes("hosting");
    if (filter === "mail") return ev.includes("mail") || ev.includes("email");
    if (filter === "system") return !ev.includes("deploy") && !ev.includes("site") && !ev.includes("mail");
    return true;
  });

  return (
    <div ref={rootRef} className="relative">
      <button
        type="button"
        aria-label={label}
        aria-expanded={open}
        aria-haspopup="dialog"
        onClick={() => setOpen((o) => !o)}
        className="relative flex h-8 w-8 items-center justify-center rounded-md text-zinc-500 hover:bg-zinc-800 hover:text-zinc-100"
      >
        {/* Inline, not an icon font: the shell must not depend on a network
            request to render its own chrome. */}
        <svg aria-hidden="true" viewBox="0 0 20 20" className="h-5 w-5" fill="currentColor">
          <path d="M10 2a5 5 0 0 0-5 5v2.6l-1.3 2.6A.8.8 0 0 0 4.4 13.5h11.2a.8.8 0 0 0 .7-1.2L15 9.6V7a5 5 0 0 0-5-5Zm0 16a2.5 2.5 0 0 0 2.4-1.8H7.6A2.5 2.5 0 0 0 10 18Z" />
        </svg>
        {unread > 0 && (
          <span
            aria-hidden="true"
            className="absolute -right-0.5 -top-0.5 min-w-[1.05rem] rounded-full bg-emerald-500 px-1 text-center text-[0.65rem] font-semibold leading-[1.05rem] text-zinc-900"
          >
            {unread > 99 ? "99+" : unread}
          </span>
        )}
      </button>

      {open && (
        <div
          role="dialog"
          aria-label="Notifications"
          className="absolute right-0 z-20 mt-2 w-96 max-w-[calc(100vw-2rem)] overflow-hidden rounded-xl border border-zinc-700 bg-zinc-900 shadow-2xl backdrop-blur-md"
        >
          <div className="flex items-center justify-between border-b border-zinc-700/80 px-3.5 py-2.5">
            <div className="flex items-center gap-2">
              <span className="text-sm font-semibold text-zinc-100">Notifications</span>
              {unread > 0 && (
                <span className="rounded-full bg-emerald-950 border border-emerald-800/60 px-2 py-0.5 text-[0.65rem] font-medium text-emerald-400">
                  {unread} unread
                </span>
              )}
            </div>
            {unread > 0 && (
              <button
                type="button"
                onClick={readAll}
                className="rounded px-2 py-1 text-xs text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 transition-colors"
              >
                Mark all read
              </button>
            )}
          </div>

          {/* Cross-App Filter Tabs */}
          <div className="flex items-center gap-1 border-b border-zinc-800 bg-zinc-950/40 px-3 py-1.5 text-xs">
            {(["all", "deploy", "mail", "system"] as const).map((tab) => (
              <button
                key={tab}
                type="button"
                onClick={() => setFilter(tab)}
                className={`rounded px-2.5 py-1 capitalize transition-colors ${
                  filter === tab
                    ? "bg-zinc-800 text-zinc-100 font-medium"
                    : "text-zinc-500 hover:text-zinc-300"
                }`}
              >
                {tab}
              </button>
            ))}
          </div>

          <ul className="max-h-96 overflow-y-auto divide-y divide-zinc-800/60">
            {items === null && <li className="px-3 py-6 text-center text-sm text-zinc-500">Loading…</li>}
            {items !== null && displayedItems.length === 0 && (
              <li className="px-3 py-8 text-center text-sm text-zinc-500">
                {filter === "all" ? "Nothing yet." : `No ${filter} notifications.`}
              </li>
            )}
            {displayedItems.map((n) => {
              const badge = eventBadge(n.event);
              return (
                <li key={n.id}>
                  <button
                    type="button"
                    onClick={() => openOne(n)}
                    className="flex w-full gap-2.5 px-3.5 py-2.5 text-left hover:bg-zinc-800/70 transition-colors"
                  >
                    <span
                      aria-hidden="true"
                      className={`mt-1.5 h-2 w-2 shrink-0 rounded-full ${n.readAt ? "bg-transparent" : "bg-emerald-500 shadow-sm shadow-emerald-500/50"}`}
                    />
                    <span className="min-w-0 flex-1">
                      <div className="flex items-center justify-between gap-1.5">
                        <span className={`block truncate text-sm font-medium ${n.readAt ? "text-zinc-400" : "text-zinc-100"}`}>
                          {n.title}
                        </span>
                        <span className={`shrink-0 rounded border px-1.5 py-0.2 text-[0.65rem] font-medium ${badge.color}`}>
                          {badge.label}
                        </span>
                      </div>
                      {n.body && <span className="block truncate text-xs text-zinc-400 mt-0.5">{n.body}</span>}
                      <time dateTime={n.createdAt} className="block text-[0.7rem] text-zinc-500 mt-1">
                        {relative(n.createdAt)}
                      </time>
                    </span>
                  </button>
                </li>
              );
            })}
          </ul>
        </div>
      )}
    </div>
  );
}

function eventBadge(event: string): { label: string; color: string } {
  const ev = (event || "").toLowerCase();
  if (ev.includes("deploy") || ev.includes("site") || ev.includes("hosting")) {
    return { label: "Deploy", color: "bg-sky-950 text-sky-400 border-sky-800/60" };
  }
  if (ev.includes("mail") || ev.includes("email")) {
    return { label: "Mail", color: "bg-violet-950 text-violet-400 border-violet-800/60" };
  }
  if (ev.includes("project") || ev.includes("task")) {
    return { label: "Project", color: "bg-amber-950 text-amber-400 border-amber-800/60" };
  }
  if (ev.includes("auth") || ev.includes("security")) {
    return { label: "Security", color: "bg-rose-950 text-rose-400 border-rose-800/60" };
  }
  return { label: "System", color: "bg-zinc-800 text-zinc-400 border-zinc-700/60" };
}

/** "4m ago" rather than an ISO string, which nobody reads as a time. */
function relative(iso: string): string {
  const then = Date.parse(iso);
  if (Number.isNaN(then)) return "";
  const secs = Math.max(0, Math.round((Date.now() - then) / 1000));
  if (secs < 60) return "just now";
  const mins = Math.round(secs / 60);
  if (mins < 60) return `${mins}m ago`;
  const hours = Math.round(mins / 60);
  if (hours < 24) return `${hours}h ago`;
  return `${Math.round(hours / 24)}d ago`;
}
