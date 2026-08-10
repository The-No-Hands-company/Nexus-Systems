import { describe, test, expect } from "vitest";
import { buildSvgDataUrl } from "./export";
import { makeElement } from "../stores/model";
import type { BoardData } from "../stores/useEditorStore";

const board: BoardData = {
  id: "b1",
  name: "test",
  description: "",
  width: 500,
  height: 400,
  background: "#ffffff",
  isPublic: false,
  defaultStyleMode: "clean",
  gridSnap: false,
  elements: [],
};

describe("buildSvgDataUrl", () => {
  test("produces a titled svg data url", () => {
    const url = buildSvgDataUrl(board, []);
    expect(url.startsWith("data:image/svg+xml;charset=utf-8,")).toBe(true);
    const svg = decodeURIComponent(url.split(",")[1]);
    expect(svg).toContain('<svg xmlns="http://www.w3.org/2000/svg"');
    expect(svg).toContain('width="500" height="400"');
  });

  test("includes clean shapes", () => {
    const rect = makeElement("rectangle", { x: 10, y: 20, width: 100, height: 50 }, { stroke: "#ff0000" });
    const ellipse = makeElement("ellipse", { x: 30, y: 40, width: 60, height: 40 });
    const line = makeElement("line", { x1: 0, y1: 0, x2: 90, y2: 90 });
    const url = buildSvgDataUrl(board, [line, rect, ellipse]);
    const svg = decodeURIComponent(url.split(",")[1]);
    expect(svg).toContain('<rect x="10" y="20"');
    expect(svg).toContain('<ellipse');
    expect(svg).toContain('<line');
    expect(svg).toContain('#ff0000');
  });

  test("respects hidden elements", () => {
    const visible = makeElement("rectangle", { x: 10, y: 20, width: 100, height: 50 });
    const hidden = makeElement("rectangle", { x: 200, y: 20, width: 100, height: 50 });
    hidden.data.hidden = true;
    const svg = decodeURIComponent(buildSvgDataUrl(board, [visible, hidden]).split(",")[1]);
    expect((svg.match(/<rect\s+x=/g) ?? []).length).toBe(1);
  });

  test("exports freehand points in the canonical [x,y,pressure] shape", () => {
    // The kernel stores freehand points as number[][] ([x, y, pressure]) —
    // toolController.freehandData, renderElement and hitTest all agree on that.
    // The exporter must read the same shape; reading {x,y} silently yields
    // points="undefined,undefined" and the stroke vanishes from the SVG.
    const stroke = makeElement("freehand", { points: [[10, 20, 0.5], [30, 40, 0.5]] });
    const svg = decodeURIComponent(buildSvgDataUrl(board, [stroke]).split(",")[1]);
    expect(svg).toContain('<polyline');
    expect(svg).toContain('points="10,20 30,40"');
    expect(svg).not.toContain("undefined");
  });

  test("sorts by order", () => {
    const a = makeElement("text", { x: 0, y: 0, text: "A" });
    const b = makeElement("text", { x: 0, y: 0, text: "B" });
    a.order = 2;
    b.order = 1;
    const svg = decodeURIComponent(buildSvgDataUrl(board, [a, b]).split(",")[1]);
    expect(svg.indexOf(">B<")).toBeLessThan(svg.indexOf(">A<"));
  });
});