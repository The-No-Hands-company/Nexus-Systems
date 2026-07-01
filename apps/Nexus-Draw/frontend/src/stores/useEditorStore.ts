import { create } from "zustand";

export interface Vec2 { x: number; y: number }

export interface ElementData {
  id: string;
  elementType: "shape" | "path" | "text" | "image" | "arrow" | "sticky";
  data: Record<string, unknown>;
  style: Record<string, unknown>;
  transform: { a: number; b: number; c: number; d: number; e: number; f: number };
  order: number;
}

export interface BoardData {
  id: string;
  name: string;
  description: string;
  width: number;
  height: number;
  background: string;
  isPublic: boolean;
  elements: ElementData[];
}

interface HistoryEntry {
  elements: ElementData[];
}

interface EditorStore {
  board: BoardData | null;
  boardId: string | null;
  elements: ElementData[];
  setBoard: (b: BoardData) => void;

  selectedElementIds: Set<string>;
  selectElement: (id: string, multi?: boolean) => void;
  deselectAll: () => void;

  activeTool: string;
  setActiveTool: (t: string) => void;

  pan: Vec2;
  zoom: number;
  setPan: (p: Vec2) => void;
  setZoom: (z: number) => void;

  sidebar: "layers" | "properties" | null;
  setSidebar: (s: "layers" | "properties" | null) => void;

  undoStack: HistoryEntry[];
  redoStack: HistoryEntry[];
  pushHistory: () => void;
  undo: () => void;
  redo: () => void;

  addElement: (el: ElementData) => void;
  updateElement: (id: string, patch: Partial<ElementData>) => void;
  removeElement: (id: string) => void;
  reorderElements: (ids: string[]) => void;
}

export const useEditorStore = create<EditorStore>((set, get) => ({
  board: null,
  boardId: null,
  elements: [],
  selectedElementIds: new Set(),
  activeTool: "select",
  pan: { x: 0, y: 0 },
  zoom: 1,
  sidebar: null,
  undoStack: [],
  redoStack: [],

  setBoard: (b) => set({ board: b, boardId: b.id, elements: b.elements, selectedElementIds: new Set(), undoStack: [], redoStack: [] }),

  selectElement: (id, multi = false) => set((s) => {
    const next = multi ? new Set(s.selectedElementIds) : new Set<string>();
    if (next.has(id)) next.delete(id); else next.add(id);
    return { selectedElementIds: next };
  }),

  deselectAll: () => set({ selectedElementIds: new Set() }),

  setActiveTool: (t) => set({ activeTool: t }),
  setPan: (p) => set({ pan: p }),
  setZoom: (z) => set({ zoom: Math.max(0.1, Math.min(10, z)) }),
  setSidebar: (s) => set({ sidebar: s }),

  pushHistory: () => set((s) => ({
    undoStack: [...s.undoStack.slice(-50), { elements: JSON.parse(JSON.stringify(s.elements)) }],
    redoStack: [],
  })),

  undo: () => set((s) => {
    if (s.undoStack.length === 0) return s;
    const prev = s.undoStack[s.undoStack.length - 1];
    return {
      undoStack: s.undoStack.slice(0, -1),
      redoStack: [...s.redoStack, { elements: JSON.parse(JSON.stringify(s.elements)) }],
      elements: prev.elements,
    };
  }),

  redo: () => set((s) => {
    if (s.redoStack.length === 0) return s;
    const next = s.redoStack[s.redoStack.length - 1];
    return {
      redoStack: s.redoStack.slice(0, -1),
      undoStack: [...s.undoStack, { elements: JSON.parse(JSON.stringify(s.elements)) }],
      elements: next.elements,
    };
  }),

  addElement: (el) => {
    get().pushHistory();
    set((s) => ({ elements: [...s.elements, el] }));
  },

  updateElement: (id, patch) => {
    get().pushHistory();
    set((s) => ({
      elements: s.elements.map((el) => (el.id === id ? { ...el, ...patch } : el)),
    }));
  },

  removeElement: (id) => {
    get().pushHistory();
    set((s) => ({
      elements: s.elements.filter((el) => el.id !== id),
      selectedElementIds: new Set([...s.selectedElementIds].filter((sid) => sid !== id)),
    }));
  },

  reorderElements: (ids) => {
    get().pushHistory();
    set((s) => {
      const map = new Map(s.elements.map((el) => [el.id, el]));
      return { elements: ids.map((id, i) => ({ ...map.get(id)!, order: i })) };
    });
  },
}));
