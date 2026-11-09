export type EventCategory = "general" | "work" | "personal" | "meeting" | "important" | "travel";

export type CategoryMeta = {
  id: EventCategory;
  label: string;
  badgeBg: string;
  badgeText: string;
  chipBg: string;
  chipText: string;
};

export const CATEGORIES: Record<EventCategory, CategoryMeta> = {
  general: {
    id: "general",
    label: "General",
    badgeBg: "bg-zinc-800 border border-zinc-700",
    badgeText: "text-zinc-300",
    chipBg: "bg-accent/20",
    chipText: "text-accent",
  },
  work: {
    id: "work",
    label: "Work",
    badgeBg: "bg-blue-950/80 border border-blue-700/60",
    badgeText: "text-blue-300",
    chipBg: "bg-blue-950/80 border border-blue-700/50",
    chipText: "text-blue-300",
  },
  personal: {
    id: "personal",
    label: "Personal",
    badgeBg: "bg-emerald-950/80 border border-emerald-700/60",
    badgeText: "text-emerald-300",
    chipBg: "bg-emerald-950/80 border border-emerald-700/50",
    chipText: "text-emerald-300",
  },
  meeting: {
    id: "meeting",
    label: "Meeting",
    badgeBg: "bg-amber-950/80 border border-amber-700/60",
    badgeText: "text-amber-300",
    chipBg: "bg-amber-950/80 border border-amber-700/50",
    chipText: "text-amber-300",
  },
  important: {
    id: "important",
    label: "Important",
    badgeBg: "bg-rose-950/80 border border-rose-700/60",
    badgeText: "text-rose-300",
    chipBg: "bg-rose-950/80 border border-rose-700/50",
    chipText: "text-rose-300",
  },
  travel: {
    id: "travel",
    label: "Travel",
    badgeBg: "bg-cyan-950/80 border border-cyan-700/60",
    badgeText: "text-cyan-300",
    chipBg: "bg-cyan-950/80 border border-cyan-700/50",
    chipText: "text-cyan-300",
  },
};

export function parseCategory(description?: string): EventCategory {
  if (!description) return "general";
  const desc = description.toLowerCase();
  for (const cat of ["work", "personal", "meeting", "important", "travel"] as const) {
    if (desc.includes(`[category:${cat}]`) || desc.includes(`#${cat}`)) return cat;
  }
  return "general";
}

export function cleanDescription(description?: string): string {
  if (!description) return "";
  return description
    .replace(/\[category:[a-z]+\]/gi, "")
    .replace(/\[reminder:[a-z0-9]+\]/gi, "")
    .trim();
}

export function parseReminder(description?: string): string {
  if (!description) return "none";
  const match = description.match(/\[reminder:([a-z0-9]+)\]/i);
  return match ? match[1] : "none";
}
