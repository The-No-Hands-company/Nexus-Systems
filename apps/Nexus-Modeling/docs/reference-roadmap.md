# Nexus Modeling · Reference & Roadmap
**Toward industry standard**  
Perfecting the modeler, pixel by pixel

A working map from where Nexus Modeling stands today to the bar set by Blender, Maya, 3ds Max, Modo, Houdini and Plasticity — and a decision about where the differentiation actually lives. Match the interaction grammar users already know; win on the learning curve and the seams between tools.

**Scope** · the modeler, before the suite  
**Principle** · standard on the core, distinctive on UX  
**Compiled** · 2026-07-09

---

## 01 · Where Nexus stands today

The kernel is strong. The shell is the gate.

Grounded in the current tree — not aspiration. The geometry kernel and the new headless visual loop are healthy; almost everything a user would touch lives in the editor layer, which does not currently build.

### Geometry kernel · Solid
1921 tests green. Booleans, bevel, remesh, half-edge, subdivision.

### Visual dev-loop · nxrender · Working
Headless render-to-PNG. Trustworthy images the AI can see & diagnose.

### Viewport shading → Standard
Flat / angle auto-smooth / real wireframe. Reversed-Z & two-sided fixed.

### Modes · 15 coded
No UI Select, Vertex/Edge/Face edit, Boolean, Extrude, Fillet… no switcher.

### Selection highlight · Stub
One method: "draw a wireframe around selected." Far from standard.

### Editor shell · Broken
Mid-refactor EditorUI.cpp doesn't build. The gate for all app work.

### ImGui · v1.91.6
Master tag — no docking branch. No dockable panels yet.

### Console · Absent
No dedicated module found; if F2 toggles one it's inline & needs promoting.

---

## 02 · The four pillars of a modeler

What "industry standard" decomposes into

Every DCC differs in flavor, but the modeling experience rests on the same four pillars. Nexus must be fluent in all four before the suite ambitions matter.

### A Selection & highlight
The single most-used interaction. The standard is: a hover pre-highlight (the element under the cursor lights up before you click — Maya calls it preselection highlight), a distinct selected color, and a brighter active element (Blender: orange selected, near-white active; the active element is the reference for many ops).

The "pixel-perfect" part is the outline itself: production apps draw a post-process silhouette off an object/ID buffer — the Jump Flood Algorithm turns the mask into a distance field for crisp, adjustable-width outlines — not a wireframe. Add box / lasso / circle select, X-ray select-through, and soft-selection falloffs (Modo, Maya) for organic work.

### B Modes & component editing
The Object ↔ Edit split is the backbone. In edit mode you select components: Blender exposes vertex / edge / face; 3ds Max's Editable Poly adds border and element (5 levels); Maya switches component masks via marking menus.

Two philosophies to borrow deliberately: 3ds Max's modifier stack (non-destructive, re-orderable history) and Houdini's node graph (fully procedural, groups = saved selections). Nexus already has a parametric feature history and a constraint graph — the raw material for a modern, non-destructive stack is present.

### C Viewport visuals
Adaptive grid & axes with distance fade, transform gizmos (screen-constant size, axis-colored, with plane and screen-space handles), and shading modes: wireframe, solid, material/matcap, rendered. Overlays for normals, statistics, and origins.

Nexus is closest to standard here — the rasterizer now shades and outlines correctly, and nxrender lets this be tuned to the pixel without the app even running.

### D Application shell & UX
Dockable, saveable panel layouts (ImGui's docking branch); mode/workspace tabs; a real console/log; and fast command access — Blender's pie menus, Maya's marking menus, Max's quad menus, or a modern command palette.

This is the layer Nexus deliberately rethinks. Plasticity is the proof point: it took Blender's grammar and wrapped it in context-sensitive pop-up widgets that first-timers grasp immediately — "CAD for artists."

---

## 03 · The competitive field

How the majors handle it

A capability matrix, not a scoreboard — the point is to see the conventions that users arrive already knowing, and where Nexus sits today.  
● strong · ◐ partial / different model · ○ absent

| Capability | Blender | Maya | 3ds Max | Modo | Plasticity | Nexus · now |
|------------|---------|------|---------|------|------------|-------------|
| Hover pre-highlight | ● on | ● preselection | ● on | ● on | ● on | ○ none |
| Selected vs active | ● orange / white | ● green / white | ● white / red | ● yes | ● yes | ○ single color |
| Component levels | ● vert/edge/face | ● +UV, multi | ● +border/element | ● vert/edge/poly | ◐ solids-first | ◐ coded, no UI |
| Silhouette outline | ● post-process | ● yes | ● yes | ● yes | ● yes | ○ wireframe stub |
| Soft-select / falloff | ● prop. edit | ● soft select | ● soft sel | ● falloffs (best) | ◐ n/a NURBS | ○ — |
| Non-destructive stack | ● modifiers | ● history | ● modifier stack | ● mesh ops | ● history | ◐ feature history exists |
| Transform gizmos | ● yes | ● yes | ● yes | ● action centers | ● yes | ◐ TransformGizmo coded |
| Dockable panels | ● areas | ● yes | ● yes | ● yes | ● yes | ○ no docking |
| Fast command access | ● pie + F3 search | ● marking menus | ● quad menus | ● pie menus | ● context widgets | ○ — |

---

## 04 · The strategic split

What to match, and where to win

The trap is reinventing the interaction grammar — muscle memory that took users years to build. Spend the originality where it compounds: the onboarding curve and the seams between tools.

### Match the standard
Table stakes · don't be clever here

- Selection model — hover-highlight, selected/active distinction, component levels, box/lasso/circle, X-ray.
- Gizmos & snapping — axis-colored move/rotate/scale, grid/vertex/edge snapping, numeric entry.
- Shading modes — wire / solid / material / rendered, with matcap studio lighting.
- Undo/redo & a non-destructive history users can trust and re-order.
- A keymap that respects the reflexes of Blender/Max refugees.

### Win on these
Nexus's edge · where originality pays

- Zero-tutorial onboarding — context-sensitive widgets & a discoverable command palette instead of a wall of memorized hotkeys.
- One coherent suite — model → sculpt → paint → engine share selection, history, and UI grammar. No re-learning per tool.
- Modern docking & workspaces from day one, not bolted on.
- The AI visual loop — the engine can see its own output and self-correct; a genuinely novel development & assist capability.
- Consistency as a feature — one mental model across the whole creative pipeline.

---

## 05 · The roadmap
A dependency-ordered path

These phases are a real sequence, not a menu — each is gated on the one before it. The recent kernel & rasterizer work already de-risks the rendering inside Phases 1–2.

### ✓ Foundation — **Done**

Kernel green (1921 tests). Headless nxrender visual loop. Rasterizer to standard: true flat shading, angle-based auto-smooth (hard cubes + smooth spheres, no topology change), real-edge wireframe, plus the reversed-Z and two-sided fixes those exposed. Editor split into nexus_app so kernel work never blocks on the UI.

Verified by eye via nxrender → **Unblocks everything below**

---

### 0 — Make the app runnable again · **The gate · next**

Resolve the broken EditorUI.cpp — finish the in-flight refactor or reset it to the last-good base and rebuild the shell intentionally. Nothing a user touches (selection display, mode tabs, console, docking) can be seen or run until nexus_modeling launches.

**Verify** · app launches, tests stay green  
**Blocks** · Phases 3–4 entirely

---

### 1 — Selection & highlight to standard · **Partly headless**

An object/ID pick buffer; hover pre-highlight; a real selected-vs-active distinction; component modes (vertex / edge / face) wired to the existing edit modes; and a Jump-Flood silhouette outline replacing the wireframe stub. The outline and component display can be prototyped and tuned in nxrender before the app is even up.

**Verify** · nxrender first, then in-app  
**Needs** · Phase 0 for in-app pick

---

### 2 — Viewport visuals · **Prototypable now**

Adaptive grid & axes with fade, transform gizmos (screen-constant, axis-colored, plane/screen handles), shading modes (wire/solid/matcap/rendered), and overlays (normals, stats, origins). Nexus is closest to standard here; much can advance headlessly.

**Verify** · nxrender + in-app  
**Leans on** · Foundation rasterizer

---

### 3 — The mode system, surfaced · **Planned**

Give the 15 already-coded modes a face: an Object ↔ Edit toggle, a component-level bar (vert/edge/face/border/element), and a mode switcher that exposes Boolean / Extrude / Fillet / Revolve as first-class. Header and status bar reflecting current mode and selection counts.

**Verify** · in-app  
**Needs** · Phase 0

---

### 4 — App shell & UX — the differentiator · **Planned**

Migrate ImGui v1.91.6 → docking branch; dockable, saveable layouts; a proper console/log panel and a command palette (the anti-tutorial); a browsable keymap; and the context-sensitive, first-timer-friendly widgets that are Nexus's reason to exist. This is where "an alternative to all of them" gets earned.

**Verify** · in-app, real users  
**Needs** · Phases 0–3

---

## Sources

Cross-DCC conventions verified against primary documentation and reference material, July 2026:

- Blender Manual — Selecting Objects
- Blender selection & active outline colors
- Maya — Highlight components before selecting (preselection)
- 3ds Max — Editable Poly sub-object levels
- Modo — Selection falloffs
- Modo — Action centers
- Plasticity — Product tour (CAD for artists)
- Ben Golus — Jump-flood silhouette outlines

---

*Inventoried July 2026*