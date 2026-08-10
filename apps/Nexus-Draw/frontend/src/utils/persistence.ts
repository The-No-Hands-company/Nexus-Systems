import type { BoardData } from "../stores/useEditorStore";
import type { ElementData } from "../stores/model";
import type { ServerBoard } from "./api";
import { serverBoardToBoardData } from "./api";

const KEY = "nexus-draw:doc:v1";
const ACTIVE_KEY = "nexus-draw:active-board";

export type PersistedDoc = {
  board: BoardData;
  elements: ElementData[];
  savedAt: number;
};

export function loadDoc(): PersistedDoc | null;
export function loadDoc(serverBoard: ServerBoard): PersistedDoc;
export function loadDoc(serverBoard?: ServerBoard): PersistedDoc | null {
  if (serverBoard) {
    return {
      board: serverBoardToBoardData(serverBoard),
      elements: serverBoard.elements,
      savedAt: Date.now(),
    };
  }
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

/**
 * The board to boot the editor from. `setBoard` seeds `store.elements` from
 * `board.elements`, but nothing keeps `board.elements` in sync while editing —
 * the live elements are persisted separately as `doc.elements`. Booting
 * straight off `doc.board` therefore restores an empty canvas and throws the
 * drawing away, so the two are re-joined here.
 */
export function bootBoard(doc: PersistedDoc | null): BoardData {
  if (!doc) return makeDefaultBoard();
  return { ...doc.board, elements: doc.elements };
}

/** Autosave delay. Long enough to coalesce a drag's per-frame updates, short
 *  enough that a reload right after a change keeps it. */
const SAVE_DEBOUNCE_MS = 400;
let saveTimer: ReturnType<typeof setTimeout> | null = null;

/**
 * Debounced `saveDoc`. The store notifies on every change, including the
 * per-frame `setElementsLive` updates a move/resize/rotate drag emits — writing
 * synchronously there would deep-clone and stringify the whole document on
 * every mousemove.
 */
export function saveDocDebounced(board: BoardData, elements: ElementData[]) {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    saveTimer = null;
    saveDoc(board, elements);
  }, SAVE_DEBOUNCE_MS);
}

/** Flushes a pending debounced save immediately (used on page hide/unload). */
export function flushSave(board: BoardData, elements: ElementData[]) {
  if (saveTimer) {
    clearTimeout(saveTimer);
    saveTimer = null;
  }
  saveDoc(board, elements);
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

export function saveLastBoardId(id: string) { try { localStorage.setItem(ACTIVE_KEY, id); } catch { /* ignore */ } }
export function loadLastBoardId(): string | null { try { return localStorage.getItem(ACTIVE_KEY); } catch { return null; } }