import type { CatalogAudience, CatalogShelf } from "../api";

export const SHELF_LABELS: Record<CatalogShelf, string> = {
  everyday: "Everyday",
  work: "Work",
  create: "Create",
  develop: "Develop",
  operate: "Operate",
  personal: "Personal",
  explore: "Explore",
};

export const AUDIENCE_LABELS: Record<CatalogAudience, string> = {
  general: "General",
  business: "Business",
  creator: "Creator",
  developer: "Developer",
  operations: "Operations",
  "power-user": "Power user",
};

export const shelfLabel = (shelf: CatalogShelf) => SHELF_LABELS[shelf];
