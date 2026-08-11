import { randomUUID } from "node:crypto";

export interface AiElement {
  id: string; elementType: string; data: Record<string, any>;
  style: Record<string, any>; transform: { a: number; b: number; c: number; d: number; e: number; f: number };
  order: number; seed: number;
}

const BASE_STYLE = { stroke: "#60a5fa", fill: "none", strokeWidth: 2, strokeStyle: "solid", opacity: 1, radius: 8, fontFamily: "ui-sans-serif, system-ui", fontSize: 18, textAlign: "left" };

function hash(s: string): number {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}

function words(prompt: string): string[] {
  return prompt.toLowerCase().split(/[^a-z0-9]+/).filter((w) => w.length > 1).slice(0, 6);
}

export function synthesizeDiagram(prompt: string, opts: { width?: number; height?: number } = {}): AiElement[] {
  const W = opts.width ?? 1200;
  const seedBase = hash(prompt.trim());
  let seed = seedBase;
  const nextSeed = () => (seed = (Math.imul(seed, 682209101) + 12345) >>> 0);
  const ws = words(prompt);
  const count = Math.max(3, Math.min(6, ws.length + 2));
  const boxW = 180, boxH = 70, gapY = 90, left = W / 2 - boxW / 2;
  const els: AiElement[] = [];
  const labels = ws.length > 0 ? ws : ["start", "process", "end"];
  for (let i = 0; i < count; i++) {
    const label = labels[i % labels.length] ?? "node";
    const y = 120 + i * (boxH + gapY);
    els.push({
      id: randomUUID(), elementType: "rectangle",
      data: { x: left, y, width: boxW, height: boxH },
      style: { ...BASE_STYLE, stroke: i % 2 === 0 ? "#60a5fa" : "#34d399", fill: "rgba(96,165,250,0.08)" },
      transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
      order: 0, seed: nextSeed(),
    });
    els.push({
      id: randomUUID(), elementType: "text",
      data: { x: left + 12, y: y + boxH / 2 - 12, width: boxW - 24, height: 30, text: label.toUpperCase() },
      style: { ...BASE_STYLE, fontSize: 14, stroke: "#e4e4e7" },
      transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
      order: 0, seed: nextSeed(),
    });
    if (i > 0) {
      const py = 110 + i * (boxH + gapY);
      els.push({
        id: randomUUID(), elementType: "arrow",
        data: { x1: W / 2, y1: py, x2: W / 2, y2: py + boxH + 20 },
        style: { ...BASE_STYLE, stroke: "#f472b6" },
        transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
        order: 0, seed: nextSeed(),
      });
    }
  }
  return els.map((e, i) => ({ ...e, order: i }));
}
