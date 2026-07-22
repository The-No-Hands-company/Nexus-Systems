# Nexus Modeling — Getting Started

**Welcome to Nexus Modeling!** This guide will take you from zero to creating your first parametric model.

---

## 📥 Installation

### Linux (Flatpak — recommended)
```bash
flatpak install flathub com.nexus.Modeling
flatpak run com.nexus.Modeling
```

### Linux (AppImage)
```bash
wget https://github.com/nexus-modeling/nexus/releases/latest/download/Nexus-Modeling-linux-x86_64.AppImage
chmod +x Nexus-Modeling-*.AppImage
./Nexus-Modeling-*.AppImage
```

### Windows (MSI Installer)
1. Download `Nexus-Modeling-Setup.exe` from [GitHub Releases](https://github.com/nexus-modeling/nexus/releases)
2. Run installer, follow wizard
3. Launch from Start Menu

### macOS (DMG)
```bash
# Via Homebrew (when available)
brew install --cask nexus-modeling

# Or download .dmg from GitHub Releases
```

### Linux (Build from Source)
```bash
# Dependencies (Ubuntu/Debian)
sudo apt install cmake g++ libvulkan-dev libglfw3-dev libglu1-mesa-dev

# Build
git clone https://github.com/nexus-modeling/nexus.git
cd nexus/apps/Nexus-Modeling
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON
cmake --build build -j$(nproc)
./build/src/kernel/nexus_modeling
```

---

## 🎯 First Launch

### 1. Launch the App
- **Linux/Flatpak**: `flatpak run com.nexus.Modeling`
- **Windows**: Start Menu → Nexus Modeling
- **macOS**: Applications → Nexus Modeling
- **CLI**: `./build/src/kernel/nexus_modeling`

### 2. First Window
You'll see:
- **Viewport** (center) — 3D workspace
- **Toolbar** (left) — Tools, primitives, modes
- **Outliner** (right) — Object tree, properties
- **Timeline** (bottom) — Animation, keyframes
- **Status Bar** (bottom) — Coordinates, selection info, mode

---

## 🎮 First Model: Box → Boolean → Export

### Step 1: Create a Box
1. **Toolbar → Primitives → Box** (or press `Shift+B`)
2. Click in viewport → drag to size → release
3. **Result**: A parametric box feature in the outliner

### Step 2: Create a Cylinder
1. **Toolbar → Primitives → Cylinder**
2. Click on box face → drag radius → drag height
2. **Result**: Cylinder intersecting the box

### Step 3: Boolean Difference
1. Select **both** objects (Box first, then Cylinder with `Shift`)
2. **Toolbar → Boolean → Difference** (or `Shift+D`)
2. **Result**: Cylinder subtracted from box — a hole!

### Step 4: Fillet the Edges
1. Select the model
2. **Mode → Fillet** (or `F`)
3. Click edge → drag to set radius
3. **Result**: Rounded edges!

### Step 5: Export to STL
1. **File → Export → STL** (or `Ctrl+E`)
2. Choose `model.stl`
3. **Done!** Ready for 3D printing

---

## 🎮 Essential Controls

### Navigation
| Action | Mouse | Keyboard |
|--------|-------|----------|
| Orbit | `MMB` drag | `Alt + LMB` |
| Pan | `Shift + MMB` | `Alt + Shift + LMB` |
| Zoom | Scroll wheel | `Ctrl + MMB` |
| Frame All | `Home` | `F` (frame selection) |
| Standard Views | Numpad `1/3/7` | `1/3/7` (Front/Right/Top) |

### Selection
| Mode | Shortcut | Description |
|------|----------|-------------|
| Object Select | `Esc` / `Q` | Click object |
| Vertex Edit | `V` | Select vertices |
| Edge Edit | `B` | Select edges |
| Face Edit | `F` | Select faces |
| Box Select | Drag `LMB` | Drag box |
| Circle Select | `C` + drag | Circle select |
| Lasso Select | `L` + drag | Freeform select |
| Add to Selection | `Shift + Click` | Add to selection |
| Remove from Selection | `Alt + Click` | Remove from selection |
| Select All | `Ctrl+A` | All visible |
| Deselect | `A` | Clear selection |

### Transform
| Tool | Shortcut | Gizmo |
|------|----------|-------|
| Move | `W` | Arrows |
| Rotate | `R` | Rings |
| Scale | `E` | Boxes |
| Local/Global | `,` | Toggle |

### Modes
| Mode | Shortcut | Description |
|------|----------|-------------|
| Select | `Esc` / `Q` | Object selection |
| Sketch | `S` | 2D constraint sketches |
| Extrude | `E` | Extrude face/profile |
| Revolve | `R` | Revolve profile |
| Fillet | `F` | Round edges |
| Modeling | `M` | Primitive placement |
| Dimension | `D` | Add dimensions |
| Boolean | `K` | Union/Diff/Intersect |
| Pattern | `P` | Linear/circular/rectangular |
| Mirror | `N` | Mirror features |
| Vertex Edit | `V` | Vertex manipulation |
| Edge Edit | `B` | Edge manipulation |
| Face Edit | `F` | Face manipulation |

### Viewport
| Action | Shortcut |
|--------|----------|
| Wireframe | `1` |
| Solid | `2` |
| Shaded | `3` |
| Material | `4` |
| Rendered | `5` |
| X-Ray | `6` |
| Orthographic | `O` / `KP5` |
| Isometric | `KP0` |
| Front | `KP1` / `1` |
| Right | `KP3` / `3` |
| Top | `KP7` / `7` |

### File & Edit
| Action | Shortcut |
|--------|----------|
| New | `Ctrl+N` |
| Open | `Ctrl+O` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` / `Ctrl+Shift+Z` |
| Copy | `Ctrl+C` |
| Paste | `Ctrl+V` |
| Duplicate | `Ctrl+D` |
| Delete | `Del` / `X` |
| Select All | `Ctrl+A` |
| Deselect | `A` |

### View
| Action | Shortcut |
|--------|----------|
| Frame All | `Home` |
| Frame Selection | `F` |
| Toggle Grid | `G` |
| Toggle Snap | `Shift+G` |
| Toggle Ortho | `KP5` / `O` |
| Toggle Lighting | `L` |
| Toggle Wireframe | `1` / `2` / `3` / `4` / `5` / `6` |
| Toggle X-Ray | `X` |
| Toggle Quad View | `Ctrl+Q` |

### Animation
| Action | Shortcut |
|--------|----------|
| Play/Pause | `Space` |
| Prev Frame | `←` |
| Next Frame | `→` |
| First Frame | `Home` |
| Last Frame | `End` |
| Set Keyframe | `I` |
| Clear Keyframe | `K` |

### Application
| Action | Shortcut |
|--------|----------|
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` / `Ctrl+Shift+Z` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Preferences | `Ctrl+,` |
| Toggle Console | `F2` |
| Toggle Performance | `F5` |
| Toggle Keybindings | `F1` |
| Toggle Panels | `F12` |
| Fullscreen | `F11` |

---

## 🎯 Quick Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│  NAVIGATION          │  SELECTION          │  TRANSFORM      │
├──────────────────────┼─────────────────────┼─────────────────┤
│ MMB          Orbit   │ LMB           Click │ W       Move    │
│ Shift+MMB    Pan     │ Shift+LMB    Add    │ E       Rotate  │
│ Scroll       Zoom    │ Alt+LMB     Remove  │ R       Scale   │
│ Home           Frame │ Ctrl+A       All    │                 │
│ 1/3/7          Views │ A               Clear │               │
└──────────────────────┴─────────────────────┴─────────────────┘
 MODES:  Esc=Select  S=Sketch  E=Extrude  R=Revolve  F=Fillet
        M=Modeling  K=Boolean  P=Pattern  N=Mirror  V/B/F=Vert/Edge/Face
```

---

## 🔧 Preferences

**Edit → Preferences** (`Ctrl+,`)

| Tab | Key Settings |
|-----|--------------|
| **Interface** | Theme (Dark/Light), Font size, Language |
| **Viewport** | Grid size, Snap increment, Ortho/Perspective default |
| **Input** | Keymap (Blender/Maya/Max/Custom), Mouse sensitivity |
| **Viewport** | Grid size, Snap increment, Ortho/Perspective default |
| **Colors** | Theme, Selection colors, Wireframe colors |
| **Files** | Autosave interval, Backup count, Default export path |
| **Advanced** | GPU backend (Vulkan/Null), Validation layers, Shader cache |

---

## 🔧 Troubleshooting

| Issue | Solution |
|-------|----------|
| **Window doesn't appear** | Run with `NEXUS_TEST_BOX=1` env var; check `glfwInit()` |
| **Vulkan error** | Update GPU drivers; try `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` |
| **ImGui not rendering** | Check `ImGui_ImplVulkan_Init` params; verify render pass |
| **High CPU** | Disable `showPerf` (F5); check `glfwSwapInterval(1)` |
| **Crash on startup** | Delete `~/.config/nexus-modeling/imgui.ini` |
| **No GPU** | Use `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` (llvmpipe) |

---

## 🆘 Getting Help

| Resource | Link |
|----------|------|
| **GitHub Issues** | https://github.com/nexus-modeling/nexus/issues |
| **Discord** | https://discord.gg/nexus-modeling |
| **API Docs** | https://docs.nexus-modeling.dev |
| **Video Tutorials** | https://youtube.com/@nexusmodeling |

---

## 🎓 Next Steps

1. **Complete the tutorial** → [First Model](beginner/first-model.md)
2. **Learn modeling basics** → [Modeling Basics](beginner/modeling-basics.md)
3. **Master Booleans** → [Boolean Operations](beginner/boolean.md)
4. **Export for 3D printing** → [Export & Import](beginner/export-import.md)

---

*Welcome to Nexus Modeling! Build something amazing.* 🚀