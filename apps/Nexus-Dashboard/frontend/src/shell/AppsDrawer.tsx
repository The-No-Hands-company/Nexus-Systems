import { useEffect, useMemo, useState, type KeyboardEvent } from "react";
import { Link, useNavigate } from "react-router-dom";
import { Overlay } from "../../../../../packages/nexus-design/src/components/ui/overlay";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import { EmptyState } from "../../../../../packages/nexus-design/src/components/ui/empty-state";
import { cn } from "../../../../../packages/nexus-design/src/components/ui/cn";
import { emptyQueryApps, filterApps } from "./filterApps";
import { readPinnedIds } from "./usePinnedApps";
import type { AppEntry } from "../api";

export default function AppsDrawer({
  apps, open, onClose,
}: { apps: AppEntry[]; open: boolean; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const [highlight, setHighlight] = useState(0);
  const navigate = useNavigate();

  // Re-read on every open rather than subscribing: pins are shell-local
  // localStorage, not shared state, and the drawer only needs today's value
  // the moment it appears.
  const pinnedIds = useMemo(() => (open ? readPinnedIds() : []), [open]);

  const results = useMemo(
    () => (query.trim() ? filterApps(apps, query) : emptyQueryApps(apps, { pinnedIds })),
    [apps, query, pinnedIds],
  );

  // A shorter result set than before can leave `highlight` pointing past the
  // end (or at the previous list's second app, now a different one) — reset
  // it whenever the results themselves change, not just when the query does.
  useEffect(() => { setHighlight(0); }, [results]);

  useEffect(() => { if (!open) { setQuery(""); } }, [open]);

  function activate(app: AppEntry | undefined) {
    // Offline entries are never activatable — the same rule the click handler
    // already enforces by rendering them as a <div>, not a <Link>.
    if (!app || app.health !== "healthy") return;
    onClose();
    navigate(app.path);
  }

  function onKeyDown(e: KeyboardEvent<HTMLInputElement>) {
    if (results.length === 0) return;
    if (e.key === "ArrowDown") {
      e.preventDefault();
      setHighlight((h) => (h + 1) % results.length);
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      setHighlight((h) => (h - 1 + results.length) % results.length);
    } else if (e.key === "Enter") {
      e.preventDefault();
      activate(results[highlight]);
    }
  }

  return (
    <Overlay open={open} onClose={onClose} label="Apps">
      <div className="border-b border-zinc-600 p-3">
        <input
          type="search"
          role="searchbox"
          autoFocus
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          onKeyDown={onKeyDown}
          placeholder="Search apps…"
          className="w-full bg-transparent text-sm text-zinc-100 outline-none placeholder:text-zinc-500"
        />
      </div>

      <ul role="listbox" aria-label="Apps" className="max-h-96 overflow-y-auto p-2">
        {results.length === 0 ? (
          <li>
            <EmptyState title="No apps match that" hint="Try part of the name, or the app id." />
          </li>
        ) : (
          results.map((a, i) => {
            const selected = i === highlight;
            return (
              <li key={a.id} role="option" aria-selected={selected}>
                {a.health === "healthy" ? (
                  <Link
                    to={a.path}
                    onClick={onClose}
                    onMouseEnter={() => setHighlight(i)}
                    data-app-entry=""
                    className={cn(
                      "flex items-center justify-between rounded px-3 py-2 text-sm text-zinc-200 hover:bg-zinc-700",
                      selected && "bg-zinc-700",
                    )}
                  >
                    <span>{a.name}</span>
                    <Pill tone="success">online</Pill>
                  </Link>
                ) : (
                  <div
                    data-app-entry=""
                    aria-disabled="true"
                    title="This app is not running"
                    onMouseEnter={() => setHighlight(i)}
                    className={cn(
                      "flex cursor-default items-center justify-between rounded px-3 py-2 text-sm text-zinc-500",
                      selected && "bg-zinc-800",
                    )}
                  >
                    <span>{a.name}</span>
                    <Pill tone="neutral">offline</Pill>
                  </div>
                )}
              </li>
            );
          })
        )}
      </ul>

      <div className="border-t border-zinc-600 p-2">
        <Link
          to="/apps"
          onClick={onClose}
          className="block rounded px-3 py-2 text-center text-sm text-zinc-400 hover:bg-zinc-700 hover:text-zinc-100"
        >
          See all {apps.length} apps
        </Link>
      </div>
    </Overlay>
  );
}
