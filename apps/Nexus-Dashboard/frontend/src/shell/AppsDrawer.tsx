import { useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { Overlay } from "../../../../../packages/nexus-design/src/components/ui/overlay";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import { EmptyState } from "../../../../../packages/nexus-design/src/components/ui/empty-state";
import { filterApps } from "./filterApps";
import type { AppEntry } from "../api";

export default function AppsDrawer({
  apps, open, onClose,
}: { apps: AppEntry[]; open: boolean; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const results = useMemo(() => filterApps(apps, query), [apps, query]);

  return (
    <Overlay open={open} onClose={onClose} label="Apps">
      <div className="border-b border-zinc-600 p-3">
        <input
          type="search"
          role="searchbox"
          autoFocus
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Search apps…"
          className="w-full bg-transparent text-sm text-zinc-100 outline-none placeholder:text-zinc-500"
        />
      </div>

      <ul className="max-h-96 overflow-y-auto p-2">
        {results.length === 0 ? (
          <li>
            <EmptyState title="No apps match that" hint="Try part of the name, or the app id." />
          </li>
        ) : (
          results.map((a) => (
            <li key={a.id}>
              {a.health === "healthy" ? (
                <Link
                  to={a.path}
                  onClick={onClose}
                  data-app-entry=""
                  className="flex items-center justify-between rounded px-3 py-2 text-sm text-zinc-200 hover:bg-zinc-700"
                >
                  <span>{a.name}</span>
                  <Pill tone="success">online</Pill>
                </Link>
              ) : (
                <div
                  data-app-entry=""
                  aria-disabled="true"
                  title="This app is not running"
                  className="flex cursor-default items-center justify-between rounded px-3 py-2 text-sm text-zinc-500"
                >
                  <span>{a.name}</span>
                  <Pill tone="neutral">offline</Pill>
                </div>
              )}
            </li>
          ))
        )}
      </ul>
    </Overlay>
  );
}
