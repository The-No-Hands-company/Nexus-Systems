# Nexus Modeling — Getting Started (Beginner Guide)

*Welcome to Nexus Modeling! This guide takes you from zero to productive in 30 minutes.*

---

## What is Nexus Modeling?

**Nexus Modeling** is a professional-grade 3D CAD/modeling application built for precision engineering, product design, and creative 3D work. It combines:

- **Exact geometry kernel** — Real CAD math, not polygon approximations
- **Modern Vulkan renderer** — GPU-accelerated, 60+ FPS at 4K
- **Non-destructive workflow** — Every operation stays editable
- **Headless automation** — Scriptable, CI-friendly, AI-ready

Think: *Blender's flexibility* + *SolidWorks' precision* + *Houdini's procedural power* — in one native, fast, native app.

---

## 5-Minute Quick Start

### 1. Launch
```bash
# Linux (Vulkan)
./build/src/kernel/nexus_modeling

# Or with a test primitive
NEXUS_TEST_BOX=1 ./build/src/kernel/nexus_modeling
```

### 2. First 30 Seconds
| Key | Action |
|-----|--------|
| `ESC` | Select mode (default) |
| `S` | Sketch mode — draw on grid |
| `E` | Extrude selected face |
| `F` | Fillet selected edge |
| `M` | Modeling mode (primitives) |
| `Q` | Face edit mode |
| `B` | Edge edit mode |
| `V` | Vertex edit mode |

**Mouse:**
| Action | Mouse |
|--------|-------|
| Orbit | Middle drag |
| Pan | Shift + Middle drag |
| Zoom | Scroll wheel |
| Select | Left click |
| Multi-select | Shift + Left click |
| Box select | Left drag |

---

## Your First Model: A Phone Stand (5 minutes)

### Step 1: Base Sketch
1. Press **`S`** → Sketch mode
2. Press **`2`** → Rectangle tool
3. Click origin, drag to **80×40 mm**
4. Press **`ESC`** → Exit sketch

### Step 2: Extrude
1. Press **`E`** → Extrude mode
2. Click the sketch → drag up **10 mm**
3. Press **`ESC`** → Done

### Step 3: Cut the Phone Slot
1. Press **`S`** → Sketch on top face
2. Press **`2`** → Rectangle
3. Draw **70×10 mm** centered
3. Press **`ESC`** → Exit sketch
4. Press **`E`** → Extrude **Cut** through all

### Step 4: Fillet Edges
1. Press **`F`** → Fillet mode
2. Click bottom edges → drag **3 mm**
4. Press **`ESC`**

### Step 5: Export for 3D Printing
**File → Export → STL** → `phone_stand.stl`

---

## Core Concepts (Mental Model)

### 1. **Exact Geometry, Not Polygons**
Nexus stores **exact math** (NURBS, analytic surfaces), not triangles.
- Boolean operations are **exact CSG** — no mesh artifacts
- Fillets are **analytic** — perfect tangent continuity
- Export to STEP/IGES for manufacturing

### 2. **Non-Destructive History**
Every operation lives in the **Feature Tree** (left panel):
- Double-click any feature → edit parameters
- Drag to reorder → model updates instantly
- Right-click → Suppress/Delete without breaking downstream

### 3. **Selection Modes**
| Mode | Key | What you select |
|------|-----|-----------------|
| Object | `ESC` | Whole features |
| Vertex | `V` | Points |
| Edge | `B` | Edges |
| Face | `Q` | Faces |

### 3. **Sketch → Feature Workflow**
```
Sketch (2D) → Constraint Solver → 3D Feature → History
```
- Sketches are **fully constrained** (blue = free, black = fixed)
- Dimensions drive geometry — change a dim, model updates
- Constraints: coincident, tangent, parallel, perpendicular, equal, midpoint

---

## Essential Shortcuts (Memorize These 10)

| Key | Action |
|-----|--------|
| `ESC` | Select mode / cancel |
| `S` | Sketch mode |
| `E` | Extrude |
| `F` | Fillet |
| `M` | Modeling (primitives) |
| `Q` | Face edit |
| `B` | Edge edit |
| `V` | Vertex edit |
| `F1` | Keybinding cheat sheet |
| `F5` | Performance overlay |

**Mouse + Keys:**
| Combo | Action |
|-------|--------|
| Middle drag | Orbit |
| Shift + Middle | Pan |
| Scroll | Zoom |
| Shift + Click | Multi-select |
| Ctrl + Drag | Box select |
| Shift + Middle | Pan view |

---

## Navigation Cheatsheet

| Goal | How |
|------|-----|
| "Reset view" | `Home` key |
| "View all" | `Home` (double-tap) |
| "Top view" | `Numpad 7` |
| "Front view" | `Numpad 1` |
| "Right view" | `Numpad 3` |
| "Isometric" | `Numpad 0` |
| "Orthographic" | `Numpad 5` / `O` |

---

## File Operations

| Action | Shortcut |
|--------|----------|
| New | `Ctrl+N` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Open | `Ctrl+O` |
| Export STL | `Ctrl+Shift+E` |
| Import | `Ctrl+I` |

**Formats:**
| Format | Import | Export |
|--------|--------|--------|
| `.nxm` (native) | ✅ | ✅ |
| STL | ✅ | ✅ |
| OBJ | ✅ | ✅ |
| glTF 2.0 | ✅ | ✅ |
| STEP | ❌ | P1 |
| STEP/IGES | P1 | P1 |

---

## Troubleshooting (First 5 Minutes)

| Problem | Fix |
|---------|-----|
| "Window doesn't appear" | Check `VK_ICD_FILENAMES` points to `lvp_icd.x86_64.json` (llvmpipe) |
| "Vulkan init failed" | Update GPU drivers; try `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json` |
| "GLFW error" | `sudo apt install libglfw3 libglu1-mesa` |
| Window opens then closes | Check `glfwInit()` — try `GLFW_CLIENT_API=GLFW_NO_API` |
| Black screen | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json` |

---

## Next Steps (Your Learning Path)

| Week | Goal | Resources |
|------|------|-----------|
| 1 | Complete phone stand, export STL, 3D print | This guide |
| 2 | Learn sketch constraints (tangent, equal, midpoint) | `docs/user/sketching.md` |
| 2 | Master fillet/chamfer/shell | `docs/user/modeling.md` |
| 3 | Parametric design — driven dimensions, equations | `docs/user/parametric.md` |
| 4 | Assemblies & mates | `docs/user/assemblies.md` |
| 3 | Export STEP for manufacturing | `docs/user/manufacturing.md` |

---

## Where to Get Help

| Channel | Purpose |
|---------|---------|
| `F1` | In-app keybinding cheat sheet |
| `F5` | Performance overlay (FPS, triangles, draw calls) |
| `docs/user/` | Complete user manual |
| `docs/developer/` | API & architecture docs |
| GitHub Issues | Bug reports, feature requests |
| Discord `#nexus-modeling` | Community help |

---

## What's Next?

| You want to... | Read this |
|----------------|-----------|
| "Draw my first sketch properly" | `docs/user/sketching.md` |
| "Understand constraints" | `docs/user/constraints.md` |
| "Make a parametric part" | `docs/user/parametric.md` |
| "Export for 3D printing" | `docs/user/3d-printing.md` |
| "Export STEP for manufacturing" | `docs/user/manufacturing.md` |
| "Report a bug" | GitHub Issues → "Bug Report" template |

---

*Welcome to Nexus Modeling. Build something precise.*

---

*Next: [Sketching Fundamentals →](sketching.md)*