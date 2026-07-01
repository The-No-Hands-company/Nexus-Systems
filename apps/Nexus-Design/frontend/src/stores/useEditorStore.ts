import { create } from "zustand";
import type { ReactNode } from "react";

export interface Vec2 { x: number; y: number }

export interface LayerData {
  id: string; name: string; type: "shape" | "text" | "image" | "component";
  visible: boolean; locked: boolean; opacity: number; order: number;
  data: Record<string, unknown>;
}

export interface FrameData {
  id: string; name: string; x: number; y: number;
  width: number; height: number; background: string;
  layers: LayerData[]; order: number;
}

export interface ProjectData {
  id: string; name: string; description: string;
  projectType: string; width: number; height: number;
  frames: FrameData[]; status: string;
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
    const frames = s.frames.map((f) =>
      f.id === frameId
        ? { ...f, layers: f.layers.map((l) => (l.id === id ? { ...l, ...patch } : l)) }
        : f
    );
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
}));
