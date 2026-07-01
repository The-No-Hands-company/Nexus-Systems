import { create } from "zustand";
import type { ReactNode } from "react";

export interface Vec2 { x: number; y: number }
export interface LayerData {
  id: string; name: string; type: "shape" | "text" | "image" | "group";
  visible: boolean; locked: boolean; opacity: number; order: number;
  transform: { a: number; b: number; c: number; d: number; e: number; f: number };
  data: Record<string, unknown>;
}
export interface ProjectData {
  id: string; name: string; width: number; height: number;
  layers: LayerData[]; projectType: string;
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
}

export const useEditorStore = create<EditorStore>((set, get) => ({
  project: null,
  projectId: null,
  selectedLayerIds: new Set(),
  activeTool: "select",
  pan: { x: 0, y: 0 },
  zoom: 1,
  sidebar: "layers",

  setProject: (p) => set({ project: p, projectId: p.id, selectedLayerIds: new Set() }),

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
}));
