import type { KeyboardEvent } from "react";
import type { CatalogAudience, CatalogShelf } from "../api";
import { AUDIENCE_LABELS, SHELF_LABELS } from "./catalogLabels";

export default function CatalogFilters({ query, shelf, audience, capability, capabilities, onQueryChange, onShelfChange, onAudienceChange, onCapabilityChange, onKeyDown }: {
  query: string;
  shelf: CatalogShelf | "all";
  audience: CatalogAudience | "all";
  capability: string | "all";
  capabilities: string[];
  onQueryChange: (value: string) => void;
  onShelfChange: (value: CatalogShelf | "all") => void;
  onAudienceChange: (value: CatalogAudience | "all") => void;
  onCapabilityChange: (value: string | "all") => void;
  onKeyDown?: (event: KeyboardEvent<HTMLInputElement>) => void;
}) {
  return (
    <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-4">
      <label className="text-sm text-zinc-400">Search
        <input autoFocus type="search" role="searchbox" aria-label="Search catalog" value={query} onChange={(e) => onQueryChange(e.target.value)} onKeyDown={onKeyDown} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2 text-zinc-100 focus-visible:outline-2 focus-visible:outline-accent" placeholder="Search apps…" />
      </label>
      <label className="text-sm text-zinc-400">Shelf
        <select aria-label="Shelf" value={shelf} onChange={(e) => onShelfChange(e.target.value as CatalogShelf | "all")} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2 text-zinc-100 focus-visible:outline-2 focus-visible:outline-accent"><option value="all">All shelves</option>{Object.entries(SHELF_LABELS).map(([value, label]) => <option key={value} value={value}>{label}</option>)}</select>
      </label>
      <label className="text-sm text-zinc-400">Audience
        <select aria-label="Audience" value={audience} onChange={(e) => onAudienceChange(e.target.value as CatalogAudience | "all")} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2 text-zinc-100 focus-visible:outline-2 focus-visible:outline-accent"><option value="all">All audiences</option>{Object.entries(AUDIENCE_LABELS).map(([value, label]) => <option key={value} value={value}>{label}</option>)}</select>
      </label>
      <label className="text-sm text-zinc-400">Capability
        <select aria-label="Capability" value={capability} onChange={(e) => onCapabilityChange(e.target.value)} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2 text-zinc-100 focus-visible:outline-2 focus-visible:outline-accent"><option value="all">All capabilities</option>{capabilities.map((value) => <option key={value} value={value}>{value}</option>)}</select>
      </label>
    </div>
  );
}
