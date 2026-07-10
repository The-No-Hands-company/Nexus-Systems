# Nexus Systems Ecosystem Vision Board (Blueprint Mode)

> **Unreal Engine Blueprint-style visual canvas to map out the current and planned architecture of the Nexus Systems monorepo.**
> Drag nodes, connect systems, create comment boxes, insert reroute dots, and save your vision board layout directly to disk.

---

## Features

- 🖥️ **Blueprint-style Visuals** — Beautiful dark-theme dual-grid background, node borders that highlight on selection, and rounded category-coded headers.
- 🔗 **Cubic Bezier Cables** — Curved, glowing connection threads that dynamically update as nodes are dragged. Cables glow when hovered or selected.
- 📐 **Interactive Wires** — Drag connections directly from the output pin (Integrates) of a system and drop it onto the input pin (Depends) of another system.
- 🟢 **Reroute Dots** — Double-click any connection line to insert a reroute node to direct cable paths around other nodes.
- 📂 **Enclosing Comment Boxes** — Create custom translucent colored boxes (groups) to group nodes. Dragging a comment box moves all nodes enclosed within it together.
- 🌳 **Auto-Arranger** — Computes a top-down hierarchical tree layout of the entire monorepo automatically, placing `Nexus-Cloud` at the root and plotting dependencies downward.
- 🎒 **App Library Drawer** — Collapsible side drawer listing all 88+ systems in `ecosystem-data.json`. Drag and drop apps directly onto the canvas to add them.
- 💾 **State Persistence** — Syncs layout coordinates, custom comment boxes, custom wire overrides, and notes to a local `layout.json` file.

---

## How to Run

1. Make sure you are in the tool's directory:
   ```bash
   cd dhts/ecosystem-vision-board
   ```
2. Start the lightweight Python server:
   ```bash
   python3 server.py
   ```
3. Open your browser and navigate to:
   ```
   http://localhost:9900
   ```

---

## Interface Controls & Shortcuts

| Action | Control / Key |
|---|---|
| **Pan Canvas** | Right-click and drag OR Middle-click and drag OR Hold Spacebar + Left-drag |
| **Zoom In/Out** | Scroll mouse wheel |
| **Reset Zoom (1:1)** | Press `Esc` or click the `1:1` zoom button |
| **Select Item** | Left-click node, cable, reroute dot, or comment box |
| **Add Wire Connection** | Click & drag from a port pin and drop on a pin of another node |
| **Sever Wire Connection** | Select a cable and press `Delete` or `Backspace` |
| **Add Reroute Dot** | Double-click on any connection cable |
| **Group Nodes** | Click `+ Comment Box`, resize it to enclose nodes, and drag header to move them |
| **Rename Comment** | Double-click the header of a comment box |
| **Delete Selected Item** | Select any node, comment box, dot, or wire and press `Delete` |

---

## Architecture Data Source

The Vision Board queries `dhts/ecosystem-visualizer/ecosystem-data.json` for the canonical list of all systems in the monorepo. 
- If a new app is added, it will automatically appear in the left **App Drawer** as a planned/scaffolded node ready to be dropped on the board.
- Manual wire cuts and new connections are stored separate from `ecosystem-data.json` inside `dhts/ecosystem-vision-board/layout.json` to preserve the visual plan independently.
