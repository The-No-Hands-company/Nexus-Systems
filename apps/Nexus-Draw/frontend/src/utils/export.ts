import rough from "roughjs";
import type { ElementData, StyleMode } from "../stores/model";
import { resolveStyleMode } from "../stores/model";
import type { BoardData } from "../stores/useEditorStore";
import { arrowHead } from "../render/geometry";
import { renderElement } from "../render/renderElement";

function triggerDownload(dataUrl: string, filename: string) {
  const a = document.createElement("a");
  a.href = dataUrl;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

function pad(n: number) {
  return n.toString().padStart(2, "0");
}

function stamp() {
  const d = new Date();
  return `${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}-${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}`;
}

function esc(s: string) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function roughPaths(el: ElementData, drawable: unknown): string {
  const sets = (drawable as { sets: { ops: { op: string; data: number[] }[] }[] }).sets;
  const segs: string[] = [];
  for (const set of sets) {
    let d = "";
    for (const op of set.ops) {
      if (op.op === "move") d += `M${op.data[0]},${op.data[1]}`;
      else if (op.op === "bcurveTo") d += `C${op.data.join(",")}`;
      else if (op.op === "lineTo") d += `L${op.data[0]},${op.data[1]}`;
      else if (op.op === "qcurveTo") d += `Q${op.data.join(",")}`;
      else if (op.op === "close") d += "Z";
    }
    segs.push(d);
  }
  return segs.join(" ");
}

function roughSvgFor(el: ElementData, mode: StyleMode): string {
  const gen = rough.generator();
  const s = el.style;
  const opts = {
    seed: el.seed,
    roughness: 1.3,
    bowing: 1,
    stroke: s.stroke,
    strokeWidth: s.strokeWidth,
    fill: s.fill === "none" ? undefined : s.fill,
    fillStyle: "hachure" as const,
  };
  let d: string;
  if (el.elementType === "rectangle") {
    const { x, y, width, height } = el.data;
    d = roughPaths(el, gen.rectangle(x, y, width, height, opts));
  } else if (el.elementType === "ellipse") {
    const { x, y, width, height } = el.data;
    d = roughPaths(el, gen.ellipse(x + width / 2, y + height / 2, width, height, opts));
  } else if (el.elementType === "line" || el.elementType === "arrow") {
    const { x1, y1, x2, y2 } = el.data;
    d = roughPaths(el, gen.line(x1, y1, x2, y2, opts));
  } else if (el.elementType === "sticky") {
    const { x, y, width, height } = el.data;
    d = roughPaths(el, gen.rectangle(x, y, width, height, { ...opts, fill: "#fde047", fillStyle: "solid", stroke: "#ca8a04", roughness: 1.2 }));
  } else {
    return "";
  }
  return d;
}

function svgElement(el: ElementData, mode: StyleMode): string {
  const s = el.style;
  const dash = s.strokeStyle === "dashed" ? 'stroke-dasharray="6 6"' : s.strokeStyle === "dotted" ? 'stroke-dasharray="1.5 4"' : "";
  const dashSep = dash ? " " : "";
  const stroke = `stroke="${s.stroke}" stroke-width="${s.strokeWidth}" stroke-linecap="round" stroke-linejoin="round" fill="none"${dash ? " " + dash : ""}`;
  const opacity = s.opacity < 1 ? ` opacity="${s.opacity}"` : "";
  const common = `stroke="${s.stroke}" stroke-width="${s.strokeWidth}"${opacity}`;
  const body: string[] = [];

  switch (el.elementType) {
    case "rectangle": {
      const { x, y, width, height } = el.data;
      const r = s.radius || 0;
      const fillAttr = s.fill && s.fill !== "none" ? ` fill="${s.fill}"` : ' fill="none"';
      if (mode === "sketch") {
        const d = roughSvgFor(el, mode);
        if (d) body.push(`<path d="${d}" ${common} fill="${s.fill !== "none" ? s.fill : "none"}" fill-opacity="${s.opacity}"/>`);
        else body.push("");
      } else {
        body.push(`<rect x="${x}" y="${y}" width="${width}" height="${height}" rx="${r}"${fillAttr} ${stroke}${opacity}/>`);
      }
      break;
    }
    case "ellipse": {
      const { x, y, width, height } = el.data;
      const fillAttr = s.fill && s.fill !== "none" ? ` fill="${s.fill}"` : ' fill="none"';
      if (mode === "sketch") {
        const d = roughSvgFor(el, mode);
        if (d) body.push(`<path d="${d}" ${common} fill="${s.fill !== "none" ? s.fill : "none"}"/>`);
        else body.push("");
      } else {
        body.push(`<ellipse cx="${x + width / 2}" cy="${y + height / 2}" rx="${width / 2}" ry="${height / 2}"${fillAttr} ${stroke}${opacity}/>`);
      }
      break;
    }
    case "line":
    case "arrow": {
      const { x1, y1, x2, y2 } = el.data;
      if (mode === "sketch") {
        const d = roughSvgFor(el, mode);
        if (d) body.push(`<path d="${d}" ${common} fill="none"/>`);
        else body.push("");
      } else {
        body.push(`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" ${stroke}${opacity}/>`);
      }
      if (el.elementType === "arrow" && mode === "clean") {
        const { p1, p2 } = arrowHead(x1, y1, x2, y2, 12 + s.strokeWidth * 2);
        body.push(`<polygon points="${x2},${y2} ${p1[0]},${p1[1]} ${p2[0]},${p2[1]}" ${common} fill="${s.stroke}"/>`);
      }
      break;
    }
    case "freehand": {
      const pts = (el.data.points as { x: number; y: number }[]).map((p) => `${p.x},${p.y}`).join(" ");
      if (pts) body.push(`<polyline points="${pts}" ${stroke} fill="none"${opacity} stroke-linejoin="round"/>`);
      break;
    }
    case "text": {
      const { x, y, text } = el.data;
      body.push(`<text x="${x}" y="${y}" font-family="${esc(s.fontFamily)}" font-size="${s.fontSize}" fill="${s.stroke}"${opacity} text-anchor="${s.textAlign === "center" ? "middle" : s.textAlign === "right" ? "end" : "start"}">${esc(text ?? "Text")}</text>`);
      break;
    }
    case "sticky": {
      const { x, y, width, height, text } = el.data;
      if (mode === "sketch") {
        const d = roughSvgFor(el, mode);
        if (d) body.push(`<path d="${d}" ${common} fill="none"/>`);
        else body.push("");
      } else {
        body.push(`<rect x="${x}" y="${y}" width="${width}" height="${height}" fill="#fde047" stroke="#ca8a04" stroke-width="${s.strokeWidth}" rx="4"${opacity}/>`);
      }
      body.push(`<text x="${x + 8}" y="${y + 8}" font-family="${esc(s.fontFamily)}" font-size="${s.fontSize}" fill="#713f12"${opacity}>${esc(text ?? "")}</text>`);
      break;
    }
    case "image": {
      const { x, y, width, height, src } = el.data;
      if (src) body.push(`<image x="${x}" y="${y}" width="${width}" height="${height}" href="${esc(src)}"${opacity}/>`);
      break;
    }
  }
  return body.join("");
}

export function downloadPNG(board: BoardData, elements: ElementData[]) {
  const w = Math.max(1, Math.round(board.width));
  const h = Math.max(1, Math.round(board.height));
  const canvas = document.createElement("canvas");
  canvas.width = w;
  canvas.height = h;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  ctx.fillStyle = board.background || "#ffffff";
  ctx.fillRect(0, 0, w, h);
  const rc = rough.canvas(canvas);
  const mode: StyleMode = board.defaultStyleMode ?? "clean";
  const sorted = [...elements].sort((a, b) => a.order - b.order);
  for (const el of sorted) renderElement(ctx, rc, el, resolveStyleMode(el, mode));
  const url = canvas.toDataURL("image/png");
  const safe = (board.name || "board").replace(/[^\w-]+/g, "_");
  triggerDownload(url, `${safe}-${stamp()}.png`);
}

export function downloadSVG(board: BoardData, elements: ElementData[]) {
  const url = buildSvgDataUrl(board, elements);
  const safe = (board.name || "board").replace(/[^\w-]+/g, "_");
  triggerDownload(url, `${safe}-${stamp()}.svg`);
}

export function buildSvgDataUrl(board: BoardData, elements: ElementData[]): string {
  const w = Math.max(1, Math.round(board.width));
  const h = Math.max(1, Math.round(board.height));
  const mode: StyleMode = board.defaultStyleMode ?? "clean";
  const sorted = [...elements].sort((a, b) => a.order - b.order);
  const inner = sorted.filter((el) => !el.data.hidden).map((el) => svgElement(el, resolveStyleMode(el, mode))).join("");
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><rect width="${w}" height="${h}" fill="${board.background || "#ffffff"}"/>${inner}</svg>`;
  return "data:image/svg+xml;charset=utf-8," + encodeURIComponent(svg);
}