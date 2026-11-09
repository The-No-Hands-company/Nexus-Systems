import { create } from "zustand";

export interface Vec2 { x: number; y: number }

export interface TypographyStyle {
  fontFamily: string;
  fontSize: number;
  fontWeight: number;
  lineHeight: number;
  letterSpacing: number;
  textAlign: "left" | "center" | "right";
}

export interface AutoLayoutConfig {
  enabled: boolean;
  direction: "row" | "column";
  gap: number;
  padding: number;
  alignItems: "flex-start" | "center" | "flex-end";
  justifyContent: "flex-start" | "center" | "space-between";
}

export interface LayerData {
  id: string;
  name: string;
  type: "shape" | "text" | "image" | "component";
  visible: boolean;
  locked: boolean;
  opacity: number;
  order: number;
  isMasterComponent?: boolean;
  masterComponentId?: string;
  typography?: TypographyStyle;
  data: Record<string, unknown>;
}

export interface FrameData {
  id: string;
  name: string;
  x: number;
  y: number;
  width: number;
  height: number;
  background: string;
  layers: LayerData[];
  order: number;
  autoLayout?: AutoLayoutConfig;
}

export interface ProjectData {
  id: string;
  name: string;
  description: string;
  projectType: string;
  width: number;
  height: number;
  frames: FrameData[];
  status: string;
}

type SidebarTab = "layers" | "properties" | null;
type ActiveTool = "select" | "hand" | "rectangle" | "ellipse" | "text" | "frame" | "component" | "prototype" | "zoom";

interface EditorStore {
  project: ProjectData | null;
  projectId: string | null;
  setProject: (p: ProjectData) => void;

  frames: FrameData[];
  selectedFrameId: string | null;
  addFrame: (frame: FrameData) => void;
  selectFrame: (id: string) => void;
  updateFrame: (id: string, patch: Partial<FrameData>) => void;

  selectedLayerIds: Set<string>;
  selectLayer: (id: string, multi?: boolean) => void;
  deselectAll: () => void;

  activeTool: ActiveTool;
  setActiveTool: (t: ActiveTool) => void;

  pan: Vec2;
  zoom: number;
  setPan: (p: Vec2) => void;
  setZoom: (z: number) => void;

  sidebar: SidebarTab;
  setSidebar: (s: SidebarTab) => void;

  addLayer: (frameId: string, layer: LayerData) => void;
  removeLayer: (frameId: string, id: string) => void;
  updateLayer: (frameId: string, id: string, patch: Partial<LayerData>) => void;
  reorderLayer: (frameId: string, id: string, newOrder: number) => void;

  // Advanced v1.0 features
  createMasterComponent: (frameId: string, layerId: string) => void;
  createInstance: (masterComponentId: string, targetFrameId?: string) => void;
  detachInstance: (frameId: string, layerId: string) => void;
  updateAutoLayout: (frameId: string, config: Partial<AutoLayoutConfig>) => void;
  exportTokens: (format: "css" | "tailwind" | "json") => string;
}

export const useEditorStore = create<EditorStore>((set, get) => ({
  project: null,
  projectId: null,
  frames: [],
  selectedFrameId: null,
  selectedLayerIds: new Set(),
  activeTool: "select",
  pan: { x: 0, y: 0 },
  zoom: 1,
  sidebar: "layers",

  setProject: (p) => set({
    project: p,
    projectId: p.id,
    frames: p.frames,
    selectedFrameId: p.frames[0]?.id ?? null,
    selectedLayerIds: new Set(),
  }),

  addFrame: (frame) => set((s) => ({
    frames: [...s.frames, frame],
    project: s.project ? { ...s.project, frames: [...s.project.frames, frame] } : s.project,
  })),

  selectFrame: (id) => set({ selectedFrameId: id, selectedLayerIds: new Set() }),

  updateFrame: (id, patch) => set((s) => ({
    frames: s.frames.map((f) => (f.id === id ? { ...f, ...patch } : f)),
    project: s.project ? {
      ...s.project,
      frames: s.project.frames.map((f) => (f.id === id ? { ...f, ...patch } : f)),
    } : s.project,
  })),

  selectLayer: (id, multi = false) => set((s) => {
    const next = multi ? new Set(s.selectedLayerIds) : new Set<string>();
    if (next.has(id)) next.delete(id); else next.add(id);
    return { selectedLayerIds: next };
  }),

  deselectAll: () => set({ selectedLayerIds: new Set() }),

  setActiveTool: (t) => set({ activeTool: t }),
  setPan: (p) => set({ pan: p }),
  setZoom: (z) => set({ zoom: Math.max(0.1, Math.min(10, z)) }),
  setSidebar: (s) => set({ sidebar: s }),

  addLayer: (frameId, layer) => set((s) => {
    const frames = s.frames.map((f) =>
      f.id === frameId ? { ...f, layers: [...f.layers, layer] } : f
    );
    return { frames, project: s.project ? { ...s.project, frames } : s.project };
  }),

  removeLayer: (frameId, id) => set((s) => {
    const frames = s.frames.map((f) =>
      f.id === frameId ? { ...f, layers: f.layers.filter((l) => l.id !== id) } : f
    );
    return { frames, project: s.project ? { ...s.project, frames } : s.project };
  }),

  updateLayer: (frameId, id, patch) => set((s) => {
    const targetLayer = s.frames.flatMap((f) => f.layers).find((l) => l.id === id);
    const isMaster = targetLayer?.isMasterComponent;

    const frames = s.frames.map((f) => ({
      ...f,
      layers: f.layers.map((l) => {
        // If updating the layer directly
        if (f.id === frameId && l.id === id) {
          return { ...l, ...patch };
        }
        // If updating a master component, propagate non-overridden data to instances
        if (isMaster && l.masterComponentId === id) {
          return {
            ...l,
            ...(patch.opacity !== undefined ? { opacity: patch.opacity } : {}),
            ...(patch.typography ? { typography: { ...l.typography, ...patch.typography } } : {}),
            ...(patch.data ? { data: { ...l.data, ...patch.data } } : {}),
          };
        }
        return l;
      }),
    }));
    return { frames, project: s.project ? { ...s.project, frames } : s.project };
  }),

  reorderLayer: (frameId, id, newOrder) => set((s) => {
    const frames = s.frames.map((f) => {
      if (f.id !== frameId) return f;
      const layers = [...f.layers];
      const idx = layers.findIndex((l) => l.id === id);
      if (idx === -1) return f;
      const [layer] = layers.splice(idx, 1);
      layer.order = newOrder;
      layers.splice(newOrder, 0, layer);
      return { ...f, layers: layers.map((l, i) => ({ ...l, order: i })) };
    });
    return { frames, project: s.project ? { ...s.project, frames } : s.project };
  }),

  createMasterComponent: (frameId, layerId) => set((s) => {
    const frames = s.frames.map((f) => {
      if (f.id !== frameId) return f;
      return {
        ...f,
        layers: f.layers.map((l) => (l.id === layerId ? { ...l, type: "component" as const, isMasterComponent: true } : l)),
      };
    });
    return { frames };
  }),

  createInstance: (masterComponentId, targetFrameId) => set((s) => {
    let master: LayerData | undefined;
    for (const f of s.frames) {
      const found = f.layers.find((l) => l.id === masterComponentId);
      if (found) { master = found; break; }
    }
    if (!master) return s;
    const destFrameId = targetFrameId || s.selectedFrameId || s.frames[0]?.id;
    if (!destFrameId) return s;
    const instanceId = crypto.randomUUID();
    const instance: LayerData = {
      ...master,
      id: instanceId,
      name: `${master.name} (Instance)`,
      isMasterComponent: false,
      masterComponentId: master.id,
      data: {
        ...master.data,
        x: ((master.data.x as number) || 0) + 20,
        y: ((master.data.y as number) || 0) + 20,
      },
    };
    const frames = s.frames.map((f) =>
      f.id === destFrameId ? { ...f, layers: [...f.layers, instance] } : f
    );
    return { frames, selectedLayerIds: new Set([instanceId]) };
  }),

  detachInstance: (frameId, layerId) => set((s) => {
    const frames = s.frames.map((f) => {
      if (f.id !== frameId) return f;
      return {
        ...f,
        layers: f.layers.map((l) => (l.id === layerId ? { ...l, type: "shape" as const, masterComponentId: undefined } : l)),
      };
    });
    return { frames };
  }),

  updateAutoLayout: (frameId, config) => set((s) => {
    const frames = s.frames.map((f) => {
      if (f.id !== frameId) return f;
      const current = f.autoLayout || {
        enabled: true,
        direction: "column" as const,
        gap: 16,
        padding: 24,
        alignItems: "flex-start" as const,
        justifyContent: "flex-start" as const,
      };
      return { ...f, autoLayout: { ...current, ...config } };
    });
    return { frames };
  }),

  exportTokens: (format) => {
    const s = get();
    const colors: Record<string, string> = {};
    const fonts: Record<string, string> = {};
    const spacing: Record<string, string> = { sm: "8px", md: "16px", lg: "24px", xl: "32px" };

    s.frames.forEach((f, idx) => {
      colors[`frame-bg-${idx + 1}`] = f.background;
      f.layers.forEach((l) => {
        if ((l.data as any)?.fill) colors[`color-${l.name.toLowerCase().replace(/\s+/g, "-")}`] = (l.data as any).fill;
        if (l.typography?.fontFamily) fonts[`font-${l.name.toLowerCase().replace(/\s+/g, "-")}`] = l.typography.fontFamily;
      });
    });

    if (format === "css") {
      let css = ":root {\n";
      Object.entries(colors).forEach(([k, v]) => { css += `  --${k}: ${v};\n`; });
      Object.entries(fonts).forEach(([k, v]) => { css += `  --${k}: "${v}", sans-serif;\n`; });
      Object.entries(spacing).forEach(([k, v]) => { css += `  --spacing-${k}: ${v};\n`; });
      css += "}\n";
      return css;
    }
    if (format === "tailwind") {
      return JSON.stringify({ theme: { extend: { colors, fontFamily: fonts, spacing } } }, null, 2);
    }
    return JSON.stringify({ colors, fonts, spacing }, null, 2);
  },
}));
