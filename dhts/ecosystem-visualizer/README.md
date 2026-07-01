# Ecosystem Visualizer v3

> **The canonical map of every Nexus Systems project.**
> Edit `ecosystem-data.json` → run `python3 build.py` → refresh browser.

---

## What it shows

| View | What you get |
|---|---|
| 🔗 **Graph** | Interactive force-directed graph. Color-coded by status, sized by connections. **Double-click any node** to focus on just that system + its neighbors. **Arrow keys** navigate between connected nodes. **Enter** enters focus mode. |
| 📋 **List** | Searchable, filterable table of all 88+ systems. Click any row to jump to graph view. |
| 📅 **Roadmap** | Systems grouped by category with **progress bars**. Categories sorted least→most complete so you see what needs work first. |

## Features

### Focus Mode (NEW v3)
Double-click any node or press Enter on a selected system. The graph zooms into just that system and its direct connections — dependencies, integrations, and "used by" relationships. Perfect for understanding how a specific system fits in. URL updates to `#focus=system-id` for sharing.

### URL Hash Routing (NEW v3)
- `index.html#nexus-ai` — opens with that system selected
- `index.html#focus=nexus-auth` — opens in focus mode on that system
- Share links with your team. Hash updates live as you navigate.

### Dependency Tree (NEW v3)
Each system's detail panel now has an expandable **recursive dependency tree** (up to 3 levels deep). Click any node in the tree to jump to it.

### Roadmap View (NEW v3)
See all systems grouped by category, sorted by completion percentage. Each category has a progress bar showing built/total. Systems within each category are ordered by status (planned → built → native).

### Keyboard Navigation (NEW v3)
- `Ctrl+K` — focus search
- `← ↑ → ↓` — navigate between connected nodes
- `Enter` — focus on selected system
- `Esc` — exit focus mode / clear selection
- `Tab` — cycle through visible systems

### Export (NEW v3)
Click **Copy MD** in any system's detail panel to copy its full details as Markdown to clipboard.

### Insights Bar (NEW v3)
Top hub systems, orphan count, overall completion percentage — always visible below the header.

### Mobile (NEW v3)
Bottom sheet design on small screens. Tap the handle to expand/collapse the sidebar. Touch-friendly throughout.

### Ecosystem-Graph Cross-Reference
Fetches `../ecosystem-graph/graph.json` on load. Shows per-system documentation coverage status ("✅ Documented (N files)" or "⚠️ Not documented").

---

## How to edit the plan

```
dhts/ecosystem-visualizer/
├── ecosystem-data.json   ← THE SOURCE OF TRUTH. Edit this.
├── app.js                ← Application logic (read by build.py)
├── build.py              ← Generator — reads data + app.js → bakes index.html
├── index.html            ← Self-contained output (open this in browser)
└── README.md             ← You are here
```

1. Edit `ecosystem-data.json` — add/update systems, categories, statuses
2. Run `python3 build.py` (or `python3 build.py --watch` for auto-rebuild)
3. Refresh browser

### Data model

```json
{
  "schemaVersion": "1.0.0",
  "organization": { "name": "...", "principles": [...], ... },
  "ecosystems": [{
    "id": "nexus-systems",
    "categories": [{
      "id": "core-platform",
      "name": "Core Platform",
      "color": "#6366f1",
      "systems": [{
        "id": "nexus-auth",
        "name": "Nexus-Auth",
        "status": "native|built|scaffolded|shell",
        "tech": "TypeScript|Rust|Python|C++|...",
        "port": 3000,
        "description": "...",
        "dependencies": ["nexus-core"],
        "integrates": ["Nexus-API"],
        "deployment": "both|self-hosted|cloud|infra|package"
      }]
    }]
  }]
}
```

### Status values

| Status | Color | Meaning |
|---|---|---|
| `native` | Purple ◆ | Native code (Rust/C++) — highest maturity |
| `built` | Green ● | Built and working |
| `scaffolded` | Amber ● | Scaffolded, work in progress |
| `shell` | Gray ● | Planned but not started |

---

## Relationship to ecosystem-graph

| | ecosystem-graph | ecosystem-visualizer |
|---|---|---|
| **Data source** | Auto-scanned from markdown metadata | Hand-curated canonical JSON |
| **Answers** | "What IS documented?" | "What SHOULD exist?" |
| **Kept?** | ✅ Yes — complementary | ✅ Yes — primary dashboard |

The visualizer fetches `ecosystem-graph/graph.json` on load to cross-reference documentation coverage. Both tools stay separate and independently useful.

---

## The No Hands Company

**Free. Open Source. Forever.**

1. Open Source — every line of code is public
2. Free Forever — no paywalls, no subscriptions, no enterprise tiers
3. Donations Only — the only accepted payment is voluntary PayPal donations
4. Self-Hosted First — every tool runs standalone via `docker compose up`
5. Federated — tools communicate across instances, no central authority
6. AI-Native — every tool has AI deeply integrated, not bolted on
7. Privacy-First — no surveillance, no telemetry, no data mining
8. Sovereign — users own their data, their infrastructure, their destiny
