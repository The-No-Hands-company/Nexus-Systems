import { Link } from "react-router-dom";
import type { AppEntry } from "../api";
import { isLauncherApp } from "./catalog";

/**
 * The app list in the sidebar.
 *
 * An offline app renders as text rather than a link, for the same reason the
 * grid does it: inviting a click that goes nowhere is worse than showing the
 * app is down.
 *
 * Every app links to its own flat path (/chat, /mail, /cloud). The path names
 * the app; whether the shell frames it or renders it itself is decided when the
 * route renders, not encoded in the link.
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
        {apps.filter(isLauncherApp).map((app) => (
          <li key={app.id}>
              <Link
                to={app.path}
                aria-current={app.id === activeId ? "page" : undefined}
                className="block rounded-md px-3 py-2 text-sm hover:bg-zinc-800 aria-[current=page]:bg-zinc-800"
              >
                {app.name}
              </Link>
          </li>
        ))}
      </ul>
    </nav>
  );
}
