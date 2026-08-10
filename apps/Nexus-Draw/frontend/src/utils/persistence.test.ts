import { describe, test, expect, beforeEach } from "vitest";
import { loadDoc, saveDoc, clearDoc, makeDefaultBoard, bootBoard } from "./persistence";
import { makeElement } from "../stores/model";

function makeStorage(): Storage {
  const m = new Map<string, string>();
  return {
    getItem: (k: string) => m.get(k) ?? null,
    setItem: (k: string, v: string) => { m.set(k, v); },
    removeItem: (k: string) => { m.delete(k); },
    clear: () => m.clear(),
    key: (i: number) => [...m.keys()][i] ?? null,
    get length() { return m.size; },
  } as Storage;
}

describe("persistence", () => {
  beforeEach(() => {
    (globalThis as any).localStorage = makeStorage();
  });

  test("round-trips a board and elements", () => {
    const board = makeDefaultBoard("My Board");
    const el = makeElement("rectangle", { x: 5, y: 6, width: 10, height: 20 }, { stroke: "#ff0000" });
    saveDoc(board, [el]);
    const doc = loadDoc();
    expect(doc).not.toBeNull();
    expect(doc!.board.name).toBe("My Board");
    expect(doc!.elements).toHaveLength(1);
    expect(doc!.elements[0].data.x).toBe(5);
    expect(doc!.elements[0].style.stroke).toBe("#ff0000");
  });

  test("returns null when empty", () => {
    expect(loadDoc()).toBeNull();
  });

  // The board a reload boots from must carry the saved elements. setBoard()
  // seeds store.elements from board.elements, but board.elements is never kept
  // in sync — the live elements live in the separate doc.elements array. Boot
  // straight off doc.board and every drawing is silently lost on refresh.
  test("bootBoard carries the saved elements onto the board", () => {
    const el = makeElement("rectangle", { x: 5, y: 6, width: 10, height: 20 });
    saveDoc(makeDefaultBoard("Persisted"), [el]);
    const doc = loadDoc()!;
    // The stored board on its own is empty — booting from it directly (the
    // regression this guards) restores a blank canvas.
    expect(doc.board.elements).toEqual([]);
    const board = bootBoard(doc);
    expect(board.name).toBe("Persisted");
    expect(board.elements).toHaveLength(1);
    expect(board.elements[0].data.x).toBe(5);
  });

  test("bootBoard falls back to a fresh empty board when nothing is stored", () => {
    const board = bootBoard(null);
    expect(board.id).toBeTruthy();
    expect(board.elements).toEqual([]);
  });

  test("clearDoc removes the doc", () => {
    saveDoc(makeDefaultBoard(), []);
    clearDoc();
    expect(loadDoc()).toBeNull();
  });

  test("makeDefaultBoard produces usable defaults", () => {
    const b = makeDefaultBoard();
    expect(b.width).toBeGreaterThan(0);
    expect(b.id).toBeTruthy();
    expect(b.elements).toEqual([]);
  });
});