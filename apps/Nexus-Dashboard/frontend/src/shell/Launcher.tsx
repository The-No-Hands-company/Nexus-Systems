import type { AppEntry } from "../api";
import { isLauncherApp } from "./catalog";
import AppLaunchLink from "../components/AppLaunchLink";
import { appMeta } from "../components/appMeta";

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
      <ul className="space-y-0.5">
        {apps.filter(isLauncherApp).map((app) => {
          const { Icon, color } = appMeta(app.id);
          return (
            <li key={app.id}>
              <AppLaunchLink
                app={app}
                aria-current={app.id === activeId ? "page" : undefined}
                className="flex items-center gap-2.5 rounded-md px-2 py-2 text-sm text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 aria-[current=page]:bg-zinc-800 aria-[current=page]:text-zinc-100 transition-colors"
              >
                <span
                  className="flex shrink-0 items-center justify-center rounded-md"
                  style={{ width: 24, height: 24, background: `color-mix(in srgb, ${color} 20%, transparent)`, color }}
                  aria-hidden
                >
                  <Icon size={14} weight="duotone" />
                </span>
                {app.name}
              </AppLaunchLink>
            </li>
          );
        })}
      </ul>
    </nav>
  );
}
