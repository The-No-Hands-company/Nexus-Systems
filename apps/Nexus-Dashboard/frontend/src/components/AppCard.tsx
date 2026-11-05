import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import type { AppEntry } from "../api";
import { shelfLabel } from "./catalogLabels";
import AppLaunchLink from "./AppLaunchLink";
import { appMeta } from "./appMeta";

function status(app: AppEntry): { label: string; tone: "neutral" | "warning" | "danger" } {
  if (app.health !== "healthy") return { label: "Offline", tone: "neutral" };
  if (app.visibility === "restricted") return { label: "Restricted", tone: "warning" };
  if (app.reviewStatus === "pending") return { label: "Pending review", tone: "warning" };
  if (app.reviewStatus === "deferred") return { label: "Deferred", tone: "warning" };
  if (app.reviewStatus === "rejected") return { label: "Unavailable", tone: "danger" };
  return { label: app.maturity === "stable" ? "Available" : app.maturity ?? "Available", tone: "neutral" };
}

function AppIcon({ id, color, size = 36 }: { id: string; color: string; size?: number }) {
  const { Icon } = appMeta(id);
  return (
    <span
      className="flex shrink-0 items-center justify-center rounded-lg"
      style={{
        width: size,
        height: size,
        background: `color-mix(in srgb, ${color} 18%, transparent)`,
        color,
      }}
      aria-hidden
    >
      <Icon size={size * 0.55} weight="duotone" />
    </span>
  );
}

export default function AppCard({ app, compact = false, showShelf = false }: { app: AppEntry; compact?: boolean; showShelf?: boolean }) {
  const canLaunch = app.health === "healthy" && app.visibility !== "restricted" && (app.reviewStatus ?? "approved") === "approved";
  const state = status(app);
  const { color } = appMeta(app.id);

  const content = (
    <div className="flex items-start gap-3">
      <AppIcon id={app.id} color={color} />
      <div className="min-w-0 flex-1">
        <div className="flex items-start justify-between gap-2">
          <h3 className="truncate font-medium text-zinc-100">{app.name}</h3>
          <Pill tone={state.tone}>{state.label}</Pill>
        </div>
        {!compact && app.description && <p className="mt-1 line-clamp-2 text-sm text-zinc-400">{app.description}</p>}
        {showShelf && <p className="mt-1.5 text-xs text-zinc-500">{shelfLabel(app.primaryShelf ?? "explore")}</p>}
      </div>
    </div>
  );

  if (canLaunch) {
    return <AppLaunchLink app={app} aria-label={app.name} className="block rounded-lg border border-zinc-700 bg-zinc-900/60 p-4 transition-colors hover:border-accent focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent">{content}</AppLaunchLink>;
  }
  return <div data-app-entry="" aria-disabled="true" title={state.label} className="rounded-lg border border-zinc-800 bg-zinc-900/40 p-4 opacity-75">{content}</div>;
}
