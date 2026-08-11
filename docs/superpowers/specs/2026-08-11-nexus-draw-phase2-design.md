# Nexus-Draw — Phase 2 Design: Connectors, Images, Templates

**Status:** Approved for build, 2026-08-11.
**Scope:** Three phase-2 features for `apps/Nexus-Draw/frontend` on top of the
completed phase-1 editor: (1) connectors that auto-route between shapes,
(2) an image tool with drag-and-drop and clipboard paste, (3) a data-defined
template library. Stays a static single-user site with optional collab — all
three features must keep working through existing persistence and collab
serialization.

---

## 1. Connectors

### 1.1 Model

New element type `"connector"` added to `ElementType` in `stores/model.ts`.

`data`:
```
{
  startId?: string,          // glued start shape id (set when drag started on a shape)
  endId?: string,            // glued end shape id (set when drag ended on a shape)
  startPoint?: { x, y },     // fixed free anchor when startId is unset
  endPoint?: { x, y },       // fixed free anchor when endId is unset
  waypoints?: { x, y }[],    // user-added intermediate points (click-click-click)
  routing: "elbow" | "straight",
}
```

### 1.2 Tool interaction (key C)

- **Drag A→B:** mousedown on a shape starts a glued connector (`startId`);
  drag auto-routes an elbow line live to the cursor; mouseup on a shape glues
  the end (`endId`); mouseup on empty canvas creates a free connector at that
  point.
- **Waypoints:** click (no drag) starts a free connector; each subsequent
  click appends a waypoint and extends the live path; Enter or double-click
  commits. Escape cancels.
- The in-progress connector renders as a `Draft` like other tools (new
  `DraftConnector` in `toolController.ts`), committed on end.

### 1.3 Routing (pure, unit-tested)

New pure function in `render/geometry.ts`:

```
routeConnector(el: ElementData, elements: ElementData[]): { x, y }[]
```

- Resolves glued endpoints to the current position of the referenced shapes:
  exit point = center of the shape's edge nearest the other endpoint.
- `elbow`: orthogonal path — exit the start edge along the routing direction,
  run to the receiving shape's entry edge, with a snapped mid-corner where the
  primary axis changes (draw.io-lite; no obstacle avoidance).
- `straight`: direct line start→end.
- Free connectors (`startPoint`/`endPoint` without ids) route between their
  fixed points, waypoints spliced in.
- If a glued id references a missing element, fall back to the stored
  `startPoint`/`endPoint` (or the element's own origin) so rendering never
  throws on stale refs (e.g. after undo or partial collab state).

### 1.4 Rendering

- `renderElement`: new `"connector"` branch. Clean mode = multi-segment
  `moveTo`/`lineTo` path with the existing `arrowHead` at the final point;
  sketch mode = one `rc.line` per segment + `rc.polygon` head (mirrors
  `renderArrow`).
- `routeConnector` is called with the live element list at draw time, so glued
  connectors follow shapes through moves/resizes/rotation and through collab
  peer edits automatically — no extra recompute hooks.

### 1.5 Hit-testing & bounds

- `hitTest.hitElement` and `elementBounds` gain an optional `elements` list
  argument (threaded from the store/canvas) so connectors can resolve their
  glued shapes; when absent they fall back to stored points.
- `hitElement` connector branch: min `distToSegment` across polyline
  segments, within `tol + strokeWidth` (same tolerance model as line/arrow).
- `elementBounds` connector branch: AABB of the polyline from
  `routeConnector`.
- Selection overlay: connectors get a bounding box + move, but no resize
  handles (their geometry derives from glued shapes / waypoints).

### 1.6 Selection / editing

- Selectable, deletable, z-ordered, copy/paste/duplicate (existing paths work
  since `translateElement`/`cloneElementOffset` treat unknown types as box
  types — must instead translate connector points).
- `translateElement`: connector branch — translate `startPoint`/`endPoint` and
  every `waypoint`. Glued connectors are **not** translated on move (they
  follow shapes); a glued connector dragged directly keeps its glue. (Simplest
  correct rule: move only applies to free connectors.)
- `resizeElement`: connector passes through unchanged (no resize handles).
- PropertiesPanel: per-connector **routing toggle** (elbow↔straight) plus the
  standard stroke/color/width/dash/style-mode controls.

## 2. Image tool

### 2.1 Tool interaction (key I)

- **Tool click:** image tool active → click canvas → native file picker →
  chosen image placed at the click point.
- **Drag-and-drop:** dropping an image file onto the canvas places it at the
  drop point (world coords), independent of active tool.
- **Paste:** Ctrl+V with an image on the clipboard places it at the current
  viewport center.
- Placed images are sized to natural dimensions capped at max 1200px longest
  edge; placed element is selected after insert.

### 2.2 Import pipeline (pure, unit-tested)

New `utils/imageImport.ts`:

```
loadImageToDataUrl(file: File): Promise<{ dataUrl, width, height }>
```

- Reads the file, decodes via an offscreen canvas, downscales so the longest
  edge ≤ 1200px (keeps aspect ratio), re-encodes as JPEG unless the source has
  transparency, in which case PNG. Returns the data URL plus final dimensions.
- Guards against non-image files and decode failures (rejects with a clear
  error the UI surfaces).

### 2.3 Rendering

- Replace the placeholder with real `drawImage` when `data.src` is present.
  Cache loaded `HTMLImageElement`s by src in a module-level map; draw the
  cached element when available, otherwise the placeholder (so un-loaded
  images degrade gracefully instead of rendering nothing).
- Images draw identically in clean and sketch modes (roughjs has no image op).
- Box hit-testing/bounds/resize/rotate already work — `image` is already in
  `BOX_TYPES`.

### 2.4 Storage & export

- `data.src` is the data URL, matching the existing `image` `data` shape; SVG
  `<image href>` and PNG export already work unchanged.
- Persistence: data URLs flow through `localStore` JSON and collab
  `writeElements` as plain strings; localStorage quota is handled by the
  existing `saveDoc` try/catch. Downscaling on import keeps stored images
  small.

## 3. Templates

### 3.1 Data model

New `src/templates/index.ts`:

```
interface Template {
  id: string;
  name: string;
  description: string;
  icon: string;                       // small glyph
  build(): ElementData[];             // elements positioned near the origin
}
```

`build()` returns elements with unique IDs and `order` ignored (recomputed on
insert). Text elements get measured `width`/`height` (reuse `measureTextSize`).

Initial set: **Flowchart, Mind Map, Org Chart, Grid, Kanban board.** Flowchart
and Org Chart include glued connectors between their shapes (referencing the
built shape IDs), showcasing feature 1.

### 3.2 UI

- New `components/TemplatesPanel.tsx`, wired into the existing sidebar system
  (`sidebar` union gains `"templates"`, TopBar gains a Templates button).
- Panel lists name + description + icon; click inserts.

### 3.3 Insertion

New `insertTemplate(template: Template)` in `stores`/`templates` layer:

- Compute insertion origin from current `pan`/`zoom` (viewport center), offset
  every element so the template's own center lands there, translate via
  `translateElement`.
- Add all elements as **one undo step** (the `addClonesLive` pattern:
  `pushHistory` + `setElementsLive` + set selection to the new ids).

## 4. Persistence & collab compatibility

- `connector` and `image` data are plain JSON; existing
  `toStored`/`writeElements` (collab) and `localStore` (local) need no changes.
- Verified by adding connector + image elements to the persistence round-trip
  test.

## 5. Testing (vitest)

- `routeConnector`: straight vs elbow paths; glued endpoints follow moved
  shapes; missing-shape fallback.
- `hitTest`: connector hit within tolerance, miss outside; bounds AABB.
- `imageImport`: downscale to ≤1200px longest edge, aspect ratio preserved,
  JPEG for opaque / PNG for transparent sources, non-image rejection.
- `templates`: every template builds valid elements (unique ids, connectors
  reference existing shape ids, no zero-size text).
- `insertTemplate`: one undo step, selection set, correct offset.
- Persistence round-trip includes connectors and images.
- Renderer paths remain visual/verified-by-build per the established
  philosophy.

## 6. Ship

`bun run check` (tsc) + `bun test` (vitest) as the gate; redeploy the frontend
per the existing Hosting flow.

## 7. Non-goals

Full obstacle-avoidance connector routing (draw.io-grade), image editing
(crop/annotate), user-defined templates, template categories/search, and
server-side image storage. All future work.
