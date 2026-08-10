import { describe, test, expect, beforeEach } from "vitest";
import { loadDoc, saveDoc, clearDoc, makeDefaultBoard } from "./persistence";
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