import { Link } from "react-router-dom";
import type { AppEntry, CatalogShelf } from "../api";
import AppCard from "./AppCard";
import { shelfLabel } from "./catalogLabels";

export default function AppShelf({ title, shelf, apps, limit, emptyMessage = "No apps in this shelf yet." }: { title?: string; shelf: CatalogShelf; apps: AppEntry[]; limit?: number; emptyMessage?: string }) {
  const visible = typeof limit === "number" ? apps.slice(0, limit) : apps;
  const heading = title ?? shelfLabel(shelf);
  return (
    <section aria-labelledby={`shelf-${shelf}`}>
      <div className="mb-3 flex items-center justify-between gap-3">
        <h2 id={`shelf-${shelf}`} className="text-[11px] font-semibold uppercase tracking-widest text-zinc-500">{heading}</h2>
        {typeof limit === "number" && apps.length > limit && <Link to={`/apps?shelf=${shelf}`} className="text-xs text-accent hover:underline focus-visible:outline-2 focus-visible:outline-accent">See all</Link>}
      </div>
      {visible.length === 0 ? <p className="text-sm text-zinc-500">{emptyMessage}</p> : <ul className="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">{visible.map((app) => <li key={app.id}><AppCard app={app} /></li>)}</ul>}
    </section>
  );
}
