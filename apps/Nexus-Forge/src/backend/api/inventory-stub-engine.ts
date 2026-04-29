import { entityResponse, listResponse } from "./contracts";

type FeatureStatus = "✅" | "🟡" | "❌";

interface InventoryFeature {
  id: string;
  slug: string;
  name: string;
  status: FeatureStatus;
  section: string;
  notes: string;
}

let cachedFeatures: InventoryFeature[] | null = null;

function slugify(input: string): string {
  return input
    .toLowerCase()
    .replace(/&/g, " and ")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/(^-|-$)/g, "");
}

function parseInventory(markdown: string): InventoryFeature[] {
  const features: InventoryFeature[] = [];
  const usedSlugs = new Map<string, number>();
  let currentSection = "Uncategorized";

  const headerNames = new Set([
    "feature",
    "capability",
    "package format",
    "depth capability",
    "platform family",
    "criterion",
    "status",
    "symbol",
    "milestone",
    "category",
    "platform",
  ]);

  const lines = markdown.split("\n");
  for (const line of lines) {
    if (line.startsWith("## ")) {
      currentSection = line.replace(/^##\s+/, "").trim();
      continue;
    }

    if (!line.startsWith("|")) {
      continue;
    }

    const cols = line.split("|").map((c) => c.trim());
    if (cols.length < 4) {
      continue;
    }

    const name = cols[1] || "";
    const status = cols[2] as FeatureStatus;
    const notes = cols[3] || "";

    if (!(status === "✅" || status === "🟡" || status === "❌")) {
      continue;
    }

    if (!name || /^-+$/.test(name) || headerNames.has(name.toLowerCase())) {
      continue;
    }

    const baseSlug = slugify(name) || "feature";
    const seen = usedSlugs.get(baseSlug) ?? 0;
    usedSlugs.set(baseSlug, seen + 1);
    const slug = seen === 0 ? baseSlug : `${baseSlug}-${seen + 1}`;

    features.push({
      id: `inv-${features.length + 1}`,
      slug,
      name,
      status,
      section: currentSection,
      notes,
    });
  }

  return features;
}

async function loadInventoryFeatures(): Promise<InventoryFeature[]> {
  if (cachedFeatures) {
    return cachedFeatures;
  }

  try {
    const content = await Bun.file("FEATURE_INVENTORY.md").text();
    cachedFeatures = parseInventory(content);
  } catch {
    cachedFeatures = [];
  }

  return cachedFeatures;
}

function toInt(value: unknown, fallback: number): number {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  return Number.isFinite(parsed) ? parsed : fallback;
}

export function registerInventoryStubEngineRoutes(app: any) {
  app.get("/api/inventory-stub-engine/summary", async () => {
    const features = await loadInventoryFeatures();
    const implemented = features.filter((f) => f.status === "✅").length;
    const stubbed = features.filter((f) => f.status === "🟡").length;
    const notYet = features.filter((f) => f.status === "❌").length;

    return entityResponse(
      {
        total: features.length,
        implemented,
        stubbed,
        notYet,
        sections: new Set(features.map((f) => f.section)).size,
      },
      "Inventory-backed parity summary",
    );
  });

  app.get("/api/inventory-stub-engine/features", async ({ query }: any) => {
    const features = await loadInventoryFeatures();
    const statusFilter = (query?.status as FeatureStatus | undefined) || undefined;
    const sectionFilter = String(query?.section || "").trim().toLowerCase();
    const limit = Math.max(1, Math.min(500, toInt(query?.limit, 100)));
    const offset = Math.max(0, toInt(query?.offset, 0));

    let filtered = features;
    if (statusFilter === "✅" || statusFilter === "🟡" || statusFilter === "❌") {
      filtered = filtered.filter((f) => f.status === statusFilter);
    }
    if (sectionFilter) {
      filtered = filtered.filter((f) => f.section.toLowerCase().includes(sectionFilter));
    }

    const items = filtered.slice(offset, offset + limit);
    return listResponse(items, `Autonomous stub catalog slice (${offset}-${offset + items.length})`);
  });

  app.get("/api/inventory-stub-engine/features/:slug", async ({ params }: any) => {
    const features = await loadInventoryFeatures();
    const slug = String(params.slug || "").trim();
    const feature = features.find((f) => f.slug === slug);
    if (!feature) {
      return { ok: false, error: "Feature not found", status: 404 };
    }

    return entityResponse(feature, "Feature stub record");
  });

  app.post("/api/inventory-stub-engine/features/:slug/stub", async ({ params, body }: any) => {
    const features = await loadInventoryFeatures();
    const slug = String(params.slug || "").trim();
    const feature = features.find((f) => f.slug === slug);
    if (!feature) {
      return { ok: false, error: "Feature not found", status: 404 };
    }

    return entityResponse(
      {
        ...feature,
        action: "stub-queued",
        previousStatus: feature.status,
        requestedBy: body?.requestedBy || "autonomous-engine",
        requestedAt: new Date().toISOString(),
      },
      "Autonomous stubbing intent recorded",
    );
  });
}