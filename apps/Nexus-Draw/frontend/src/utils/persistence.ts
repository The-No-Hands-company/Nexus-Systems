import type { BoardData } from "../stores/useEditorStore";
import type { ElementData } from "../stores/model";

const KEY = "nexus-draw:doc:v1";

export type PersistedDoc = {
  board: BoardData;
  elements: ElementData[];
  savedAt: number;
};

export function loadDoc(): PersistedDoc | null {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return null;
    const doc = JSON.parse(raw) as PersistedDoc;
    if (!doc.board || !Array.isArray(doc.elements)) return null;
    doc.elements = doc.elements.map((el) => ({ ...el, data: { ...el.data } }));
    return doc;
  } catch {
    return null;
  }
}

export function saveDoc(board: BoardData, elements: ElementData[]) {
  try {
    const doc: PersistedDoc = {
      board: JSON.parse(JSON.stringify(board)),
      elements: JSON.parse(JSON.stringify(elements)),
      savedAt: Date.now(),
    };
    localStorage.setItem(KEY, JSON.stringify(doc));
  } catch {
    // storage full/unavailable — ignore
  }
}

export function clearDoc() {
  try {
    localStorage.removeItem(KEY);
  } catch {
    // ignore
  }
}

export function makeDefaultBoard(name = "Untitled Board"): BoardData {
  return {
    id: crypto.randomUUID(),
    name,
    description: "",
    width: 1920,
    height: 1080,
    background: "#1a1a2e",
    isPublic: false,
    defaultStyleMode: "clean",
    gridSnap: false,
    elements: [],
  };
}