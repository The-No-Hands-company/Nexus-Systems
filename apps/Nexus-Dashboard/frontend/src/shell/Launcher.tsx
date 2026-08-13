import { Link } from "react-router-dom";
import type { AppEntry } from "../api";

/**
 * The app list in the sidebar.
 *
 * An offline app renders as text rather than a link, for the same reason the
 * grid does it: inviting a click that goes nowhere is worse than showing the
 * app is down.
 */
export default function Launcher({
  apps,
  activeId,
}: {
  apps: AppEntry[];
  activeId?: string;
}) {
  return (
    <nav aria-label="App launcher" className="p-2">
      <ul className="space-y-1">
        {apps.map((app) => (
          <li key={app.id}>
            {app.health === "healthy" ? (
              <Link
                to={`/a/${app.id}`}
                aria-current={app.id === activeId ? "page" : undefined}
                className="block rounded-md px-3 py-2 text-sm hover:bg-bg-elevated aria-[current=page]:bg-bg-elevated"
              >
                {app.name}
              </Link>
            ) : (
              <span
                className="block cursor-default rounded-md px-3 py-2 text-sm text-text-muted"
                title="This app is not running"
              >
                {app.name}
              </span>
            )}
          </li>
        ))}
      </ul>
    </nav>
  );
}
