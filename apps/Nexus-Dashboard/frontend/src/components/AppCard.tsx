import { Link } from "react-router-dom";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import type { AppEntry } from "../api";
import { shelfLabel } from "./catalogLabels";

function status(app: AppEntry): { label: string; tone: "neutral" | "warning" | "danger" } {
  if (app.health !== "healthy") return { label: "Offline", tone: "neutral" };
  if (app.visibility === "restricted") return { label: "Restricted", tone: "warning" };
  if (app.reviewStatus === "pending") return { label: "Pending review", tone: "warning" };
  if (app.reviewStatus === "deferred") return { label: "Deferred", tone: "warning" };
  if (app.reviewStatus === "rejected") return { label: "Unavailable", tone: "danger" };
  return { label: app.maturity === "stable" ? "Available" : app.maturity ?? "Available", tone: "neutral" };
}

export default function AppCard({ app, compact = false, showShelf = false }: { app: AppEntry; compact?: boolean; showShelf?: boolean }) {
  const canLaunch = app.health === "healthy" && app.visibility !== "restricted" && (app.reviewStatus ?? "approved") === "approved";
  const state = status(app);
  const content = (
    <>
      <div className="flex min-w-0 items-start justify-between gap-3">
        <div className="min-w-0">
          <h3 className="truncate font-medium text-zinc-100">{app.name}</h3>
          {!compact && app.description && <p className="mt-1 line-clamp-2 text-sm text-zinc-400">{app.description}</p>}
        </div>
        <Pill tone={state.tone}>{state.label}</Pill>
      </div>
      {showShelf && <p className="mt-2 text-xs text-zinc-500">{shelfLabel(app.primaryShelf ?? "explore")}</p>}
    </>
  );

  if (canLaunch) {
    return <Link to={app.path} aria-label={app.name} className="block rounded-lg border border-zinc-700 bg-zinc-900/60 p-4 transition-colors hover:border-accent focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent">{content}</Link>;
  }
  return <div data-app-entry="" aria-disabled="true" title={state.label} className="rounded-lg border border-zinc-800 bg-zinc-900/40 p-4 opacity-75">{content}</div>;
}
