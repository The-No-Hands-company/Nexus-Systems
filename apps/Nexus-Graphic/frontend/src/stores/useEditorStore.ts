import { create } from "zustand";

export interface Vec2 { x: number; y: number }

export interface PathNode {
  id: string;
  x: number;
  y: number;
  type: "corner" | "smooth";
  handleIn?: { x: number; y: number };
  handleOut?: { x: number; y: number };
}

export interface GradientConfig {
  type: "linear" | "radial";
  angle: number;
  stops: Array<{ offset: number; color: string }>;
}

export interface LayerData {
  id: string;
  name: string;
  type: "shape" | "path" | "text" | "image" | "group";
  visible: boolean;
  locked: boolean;
  opacity: number;
  order: number;
  transform: { a: number; b: number; c: number; d: number; e: number; f: number };
  nodes?: PathNode[];
  selectedNodeIndex?: number | null;
  gradient?: GradientConfig;
  data: Record<string, unknown>;
}

export interface ProjectData {
  id: string;
  name: string;
  width: number;
  height: number;
  layers: LayerData[];
  projectType: string;
}

interface EditorStore {
  // Project
  project: ProjectData | null;
  projectId: string | null;
  setProject: (p: ProjectData) => void;

  // Selection
  selectedLayerIds: Set<string>;
  selectLayer: (id: string, multi?: boolean) => void;
  deselectAll: () => void;

  // Node editing mode
  selectedNodeIndex: number | null;
  selectNode: (idx: number | null) => void;

  // Tool
  activeTool: string;
  setActiveTool: (t: string) => void;

  // Viewport
  pan: Vec2;
  zoom: number;
  setPan: (p: Vec2) => void;
  setZoom: (z: number) => void;

  // UI
  sidebar: "layers" | "properties" | "templates" | null;
  setSidebar: (s: "layers" | "properties" | "templates" | null) => void;

  // Layer operations
  addLayer: (layer: LayerData) => void;
  removeLayer: (id: string) => void;
  updateLayer: (id: string, patch: Partial<LayerData>) => void;
  reorderLayer: (id: string, newOrder: number) => void;

  // Node and Path operations
  updateNode: (layerId: string, nodeIndex: number, patch: Partial<PathNode>) => void;
  addNode: (layerId: string, node: PathNode) => void;
  removeNode: (layerId: string, nodeIndex: number) => void;

  // Gradient operations
  updateGradient: (layerId: string, gradient: Partial<GradientConfig> | null) => void;

  // Precision SVG IO
  exportSvg: () => string;
  importSvg: (svgString: string) => void;
}

export const useEditorStore = create<EditorStore>((set, get) => ({
  project: null,
  projectId: null,
  selectedLayerIds: new Set(),
  selectedNodeIndex: null,
  activeTool: "select",
  pan: { x: 0, y: 0 },
  zoom: 1,
  sidebar: "layers",

  setProject: (p) => set({ project: p, projectId: p.id, selectedLayerIds: new Set(), selectedNodeIndex: null }),

  selectLayer: (id, multi = false) => set((s) => {
    const next = multi ? new Set(s.selectedLayerIds) : new Set<string>();
    if (next.has(id)) next.delete(id); else next.add(id);
    return { selectedLayerIds: next, selectedNodeIndex: null };
  }),

  deselectAll: () => set({ selectedLayerIds: new Set(), selectedNodeIndex: null }),

  selectNode: (idx) => set({ selectedNodeIndex: idx }),

  setActiveTool: (t) => set({ activeTool: t }),
  setPan: (p) => set({ pan: p }),
  setZoom: (z) => set({ zoom: Math.max(0.1, Math.min(10, z)) }),
  setSidebar: (s) => set({ sidebar: s }),

  addLayer: (layer) => set((s) => {
    if (!s.project) return s;
    return { project: { ...s.project, layers: [...s.project.layers, layer] } };
  }),

  removeLayer: (id) => set((s) => {
    if (!s.project) return s;
    return { project: { ...s.project, layers: s.project.layers.filter((l) => l.id !== id) } };
  }),

  updateLayer: (id, patch) => set((s) => {
    if (!s.project) return s;
    return {
      project: {
        ...s.project,
        layers: s.project.layers.map((l) => (l.id === id ? { ...l, ...patch } : l)),
      },
    };
  }),

  reorderLayer: (id, newOrder) => set((s) => {
    if (!s.project) return s;
    const layers = [...s.project.layers];
    const idx = layers.findIndex((l) => l.id === id);
    if (idx === -1) return s;
    const [layer] = layers.splice(idx, 1);
    layer.order = newOrder;
    layers.splice(newOrder, 0, layer);
    return { project: { ...s.project, layers: layers.map((l, i) => ({ ...l, order: i })) } };
  }),

  updateNode: (layerId, nodeIndex, patch) => set((s) => {
    if (!s.project) return s;
    return {
      project: {
        ...s.project,
        layers: s.project.layers.map((l) => {
          if (l.id !== layerId || !l.nodes) return l;
          const newNodes = l.nodes.map((n, i) => (i === nodeIndex ? { ...n, ...patch } : n));
          return { ...l, nodes: newNodes };
        }),
      },
    };
  }),

  addNode: (layerId, node) => set((s) => {
    if (!s.project) return s;
    return {
      project: {
        ...s.project,
        layers: s.project.layers.map((l) => {
          if (l.id !== layerId) return l;
          const currentNodes = l.nodes || [];
          return { ...l, nodes: [...currentNodes, node] };
        }),
      },
    };
  }),

  removeNode: (layerId, nodeIndex) => set((s) => {
    if (!s.project) return s;
    return {
      project: {
        ...s.project,
        layers: s.project.layers.map((l) => {
          if (l.id !== layerId || !l.nodes) return l;
          return { ...l, nodes: l.nodes.filter((_, i) => i !== nodeIndex) };
        }),
      },
      selectedNodeIndex: null,
    };
  }),

  updateGradient: (layerId, gradient) => set((s) => {
    if (!s.project) return s;
    return {
      project: {
        ...s.project,
        layers: s.project.layers.map((l) => {
          if (l.id !== layerId) return l;
          if (!gradient) return { ...l, gradient: undefined };
          const current = l.gradient || {
            type: "linear" as const,
            angle: 90,
            stops: [
              { offset: 0, color: "#6366f1" },
              { offset: 100, color: "#ec4899" },
            ],
          };
          return { ...l, gradient: { ...current, ...gradient } };
        }),
      },
    };
  }),

  exportSvg: () => {
    const s = get();
    if (!s.project) return "";
    const w = s.project.width || 800;
    const h = s.project.height || 600;

    let svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" width="${w}" height="${h}">\n`;
    svg += `  <defs>\n`;

    // Gradients defs
    s.project.layers.forEach((l) => {
      if (l.gradient) {
        if (l.gradient.type === "linear") {
          const angleRad = ((l.gradient.angle || 0) * Math.PI) / 180;
          const x1 = Math.round(50 - Math.cos(angleRad) * 50);
          const y1 = Math.round(50 - Math.sin(angleRad) * 50);
          const x2 = Math.round(50 + Math.cos(angleRad) * 50);
          const y2 = Math.round(50 + Math.sin(angleRad) * 50);
          svg += `    <linearGradient id="grad-${l.id}" x1="${x1}%" y1="${y1}%" x2="${x2}%" y2="${y2}%">\n`;
          l.gradient.stops.forEach((st) => {
            svg += `      <stop offset="${st.offset}%" stop-color="${st.color}" />\n`;
          });
          svg += `    </linearGradient>\n`;
        } else {
          svg += `    <radialGradient id="grad-${l.id}" cx="50%" cy="50%" r="50%">\n`;
          l.gradient.stops.forEach((st) => {
            svg += `      <stop offset="${st.offset}%" stop-color="${st.color}" />\n`;
          });
          svg += `    </radialGradient>\n`;
        }
      }
    });
    svg += `  </defs>\n`;

    // Render layers
    s.project.layers
      .filter((l) => l.visible)
      .forEach((l) => {
        const fill = l.gradient ? `url(#grad-${l.id})` : (l.data.fill as string) || "#3b82f6";
        const stroke = (l.data.stroke as string) || "none";
        const strokeWidth = (l.data.strokeWidth as number) || 1;
        const opacity = l.opacity !== undefined ? l.opacity : 1;

        if (l.nodes && l.nodes.length > 0) {
          let d = `M ${l.nodes[0].x} ${l.nodes[0].y}`;
          for (let i = 1; i < l.nodes.length; i++) {
            const n = l.nodes[i];
            if (n.type === "smooth" && n.handleIn && n.handleOut) {
              d += ` C ${n.handleIn.x} ${n.handleIn.y}, ${n.handleOut.x} ${n.handleOut.y}, ${n.x} ${n.y}`;
            } else {
              d += ` L ${n.x} ${n.y}`;
            }
          }
          d += " Z";
          svg += `  <path id="${l.id}" d="${d}" fill="${fill}" stroke="${stroke}" stroke-width="${strokeWidth}" opacity="${opacity}" />\n`;
        } else {
          const x = (l.data.x as number) || 0;
          const y = (l.data.y as number) || 0;
          const width = (l.data.width as number) || 100;
          const height = (l.data.height as number) || 100;
          svg += `  <rect id="${l.id}" x="${x}" y="${y}" width="${width}" height="${height}" fill="${fill}" stroke="${stroke}" stroke-width="${strokeWidth}" opacity="${opacity}" />\n`;
        }
      });

    svg += `</svg>`;
    return svg;
  },

  importSvg: (svgString) => {
    const s = get();
    if (!s.project) return;
    try {
      const parser = new DOMParser();
      const doc = parser.parseFromString(svgString, "image/svg+xml");
      const paths = doc.querySelectorAll("path, rect, circle");
      const newLayers: LayerData[] = [];

      paths.forEach((el, index) => {
        const id = crypto.randomUUID();
        const fill = el.getAttribute("fill") || "#3b82f6";
        const stroke = el.getAttribute("stroke") || "none";
        const strokeWidth = parseFloat(el.getAttribute("stroke-width") || "1") || 1;
        const name = `Imported Path ${index + 1}`;

        if (el.tagName.toLowerCase() === "rect") {
          const x = parseFloat(el.getAttribute("x") || "0") || 0;
          const y = parseFloat(el.getAttribute("y") || "0") || 0;
          const width = parseFloat(el.getAttribute("width") || "100") || 100;
          const height = parseFloat(el.getAttribute("height") || "100") || 100;
          newLayers.push({
            id,
            name,
            type: "shape",
            visible: true,
            locked: false,
            opacity: 1,
            order: s.project?.layers.length ? s.project.layers.length + index : index,
            transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
            data: { x, y, width, height, fill, stroke, strokeWidth },
          });
        } else {
          // Path with sample nodes
          newLayers.push({
            id,
            name,
            type: "path",
            visible: true,
            locked: false,
            opacity: 1,
            order: s.project?.layers.length ? s.project.layers.length + index : index,
            transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
            nodes: [
              { id: crypto.randomUUID(), x: 50, y: 50, type: "corner" },
              { id: crypto.randomUUID(), x: 150, y: 50, type: "smooth" },
              { id: crypto.randomUUID(), x: 100, y: 150, type: "corner" },
            ],
            data: { fill, stroke, strokeWidth },
          });
        }
      });

      if (newLayers.length > 0) {
        set({
          project: {
            ...s.project,
            layers: [...s.project.layers, ...newLayers],
          },
          selectedLayerIds: new Set([newLayers[0].id]),
        });
      }
    } catch (e) {
      console.error("Failed to parse SVG", e);
    }
  },
}));
