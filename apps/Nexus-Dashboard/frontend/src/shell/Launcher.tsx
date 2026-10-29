import type { AppEntry } from "../api";
import { isLauncherApp } from "./catalog";
import AppLaunchLink from "../components/AppLaunchLink";

/**
 * The app list in the sidebar.
 *
 * An offline app renders as text rather than a link, for the same reason the
 * grid does it: inviting a click that goes nowhere is worse than showing the
 * app is down.
 *
 * A shell-native or framed app links to its own flat path (/chat, /mail, /cloud)
 * and renders in-shell; an external app opens in a new tab. AppLaunchLink makes
 * that choice from the app's delivery (see the frontend load architecture note).
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
              <AppLaunchLink
                app={app}
                aria-current={app.id === activeId ? "page" : undefined}
                className="block rounded-md px-3 py-2 text-sm hover:bg-zinc-800 aria-[current=page]:bg-zinc-800"
              >
                {app.name}
              </AppLaunchLink>
          </li>
        ))}
      </ul>
    </nav>
  );
}
