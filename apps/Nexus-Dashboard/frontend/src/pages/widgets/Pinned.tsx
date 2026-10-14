import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { listApps, type AppEntry } from "../../api";
import { usePinnedApps } from "../../shell/usePinnedApps";
import WidgetShell, { type WidgetState } from "./WidgetShell";

/**
 * User-chosen pins, persisted shell-local (same footing as density — see
 * usePinnedApps.ts). Falls back to healthy apps when nothing is pinned yet,
 * so the widget is never empty just because nobody has made a choice.
 */
export default function Pinned() {
  const [state, setState] = useState<WidgetState<AppEntry[]>>({ status: "loading" });
  const [pinnedIds, togglePinned] = usePinnedApps();

  useEffect(() => {
    let cancelled = false;
    listApps()
      .then((apps) => { if (!cancelled) setState({ status: "ready", data: apps }); })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Apps unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Pinned" state={state} empty="No apps pinned yet">
      {(apps) => {
        const healthy = apps.filter((a) => a.health === "healthy");
        const pinned = pinnedIds
          .map((id) => healthy.find((a) => a.id === id))
          .filter((a): a is AppEntry => Boolean(a));
        const shown = pinned.length > 0 ? pinned : healthy;

        return shown.length === 0 ? null : (
          <ul className="grid grid-cols-2 gap-2 sm:grid-cols-3">
            {shown.map((a) => {
              const isPinned = pinnedIds.includes(a.id);
              return (
                <li key={a.id} className="group relative">
                  <Link
                    to={a.path}
                    className="flex flex-col gap-2 rounded-lg border border-zinc-700 bg-zinc-800/40 p-3 transition-colors hover:border-zinc-600 hover:bg-zinc-800"
                  >
                    <div className="flex h-8 w-8 items-center justify-center rounded-md bg-zinc-700 text-xs font-semibold uppercase text-zinc-300">
                      {a.name.slice(0, 2)}
                    </div>
                    <span className="line-clamp-1 text-sm font-medium text-zinc-200 group-hover:text-accent">
                      {a.name}
                    </span>
                  </Link>
                  <button
                    type="button"
                    onClick={() => togglePinned(a.id)}
                    aria-pressed={isPinned}
                    aria-label={isPinned ? `Unpin ${a.name}` : `Pin ${a.name}`}
                    className="absolute right-1.5 top-1.5 rounded px-1 py-0.5 text-[10px] text-zinc-600 opacity-0 transition-opacity hover:text-accent group-hover:opacity-100"
                  >
                    {isPinned ? "✕" : "＋"}
                  </button>
                </li>
              );
            })}
          </ul>
        );
      }}
    </WidgetShell>
  );
}
