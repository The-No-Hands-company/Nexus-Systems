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
          <ul className="flex flex-col gap-1">
            {shown.map((a) => {
              const isPinned = pinnedIds.includes(a.id);
              return (
                <li
                  key={a.id}
                  className="flex items-center justify-between gap-2 border-b border-zinc-700 py-1 text-sm last:border-0"
                >
                  <Link to={a.path} className="text-zinc-200 hover:text-accent">{a.name}</Link>
                  <button
                    type="button"
                    onClick={() => togglePinned(a.id)}
                    aria-pressed={isPinned}
                    aria-label={isPinned ? `Unpin ${a.name}` : `Pin ${a.name}`}
                    className="shrink-0 text-xs text-zinc-500 hover:text-accent"
                  >
                    {isPinned ? "Unpin" : "Pin"}
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
