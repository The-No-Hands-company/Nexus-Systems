import type { RoughCanvas } from "roughjs/bin/canvas";
import type { Options } from "roughjs/bin/core";
import { getStroke } from "perfect-freehand";
import type { ElementData, ElementStyle, StyleMode } from "../stores/model";
import { arrowHead } from "./geometry";

const ROUGHNESS = 1.6;
const STICKY_FALLBACK_FILL = "#fef08a";
const IMAGE_PLACEHOLDER_FILL = "rgba(148,163,184,0.15)";
const IMAGE_PLACEHOLDER_STROKE = "#64748b";

/** Build roughjs Options for an element, allowing per-shape overrides (e.g. no fill on lines). */
function roughOptions(el: ElementData, overrides: Partial<Options> = {}): Options {
  const { style, seed } = el;
  return {
    seed,
    roughness: ROUGHNESS,
    stroke: style.stroke,
    strokeWidth: style.strokeWidth,
    fill: style.fill && style.fill !== "none" ? style.fill : undefined,
    fillStyle: "hachure",
    ...overrides,
  };
}

/** Apply strokeWidth/strokeStyle/dash pattern to ctx for native (clean-mode) drawing. */
function applyStrokeStyle(ctx: CanvasRenderingContext2D, style: ElementStyle): void {
  ctx.lineWidth = style.strokeWidth;
  ctx.strokeStyle = style.stroke;
  if (style.strokeStyle === "dashed") {
    ctx.setLineDash([style.strokeWidth * 3, style.strokeWidth * 2]);
  } else if (style.strokeStyle === "dotted") {
    ctx.setLineDash([style.strokeWidth, style.strokeWidth * 2]);
  } else {
    ctx.setLineDash([]);
  }
}

function renderBox(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode
): void {
  const d = el.data as { x?: number; y?: number; width?: number; height?: number };
  const x = d.x ?? 0;
  const y = d.y ?? 0;
  const width = d.width ?? 0;
  const height = d.height ?? 0;
  const style = el.style;
  const isSticky = el.elementType === "sticky";
  const fill = style.fill && style.fill !== "none" ? style.fill : isSticky ? STICKY_FALLBACK_FILL : "none";

  if (mode === "sketch") {
    rc.rectangle(x, y, width, height, roughOptions(el, { fill: fill === "none" ? undefined : fill }));
    return;
  }

  ctx.save();
  applyStrokeStyle(ctx, style);
  ctx.beginPath();
  const radius = isSticky ? 0 : Math.max(0, Math.min(style.radius, width / 2, height / 2));
  if (radius > 0) {
    ctx.roundRect(x, y, width, height, radius);
  } else {
    ctx.rect(x, y, width, height);
  }
  if (fill !== "none") {
    ctx.fillStyle = fill;
    ctx.fill();
  }
  ctx.stroke();
  ctx.restore();
}

function renderEllipse(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode
): void {
  const d = el.data as { x?: number; y?: number; width?: number; height?: number };
  const x = d.x ?? 0;
  const y = d.y ?? 0;
  const width = d.width ?? 0;
  const height = d.height ?? 0;
  const cx = x + width / 2;
  const cy = y + height / 2;
  const style = el.style;

  if (mode === "sketch") {
    rc.ellipse(cx, cy, width, height, roughOptions(el));
    return;
  }

  ctx.save();
  applyStrokeStyle(ctx, style);
  ctx.beginPath();
  ctx.ellipse(cx, cy, Math.abs(width) / 2, Math.abs(height) / 2, 0, 0, Math.PI * 2);
  if (style.fill !== "none") {
    ctx.fillStyle = style.fill;
    ctx.fill();
  }
  ctx.stroke();
  ctx.restore();
}

function renderLine(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode
): void {
  const d = el.data as { x1?: number; y1?: number; x2?: number; y2?: number };
  const x1 = d.x1 ?? 0;
  const y1 = d.y1 ?? 0;
  const x2 = d.x2 ?? 0;
  const y2 = d.y2 ?? 0;
  const style = el.style;

  if (mode === "sketch") {
    rc.line(x1, y1, x2, y2, roughOptions(el, { fill: undefined }));
    return;
  }

  ctx.save();
  applyStrokeStyle(ctx, style);
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
  ctx.restore();
}

function renderArrow(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode
): void {
  const d = el.data as { x1?: number; y1?: number; x2?: number; y2?: number };
  const x1 = d.x1 ?? 0;
  const y1 = d.y1 ?? 0;
  const x2 = d.x2 ?? 0;
  const y2 = d.y2 ?? 0;
  const style = el.style;
  const headSize = Math.max(12, style.strokeWidth * 5);
  const { p1, p2 } = arrowHead(x1, y1, x2, y2, headSize);

  if (mode === "sketch") {
    const opts = roughOptions(el, { fill: undefined });
    rc.line(x1, y1, x2, y2, opts);
    rc.polygon(
      [
        [x2, y2],
        p1,
        p2,
      ],
      { ...opts, fill: style.stroke, fillStyle: "solid" }
    );
    return;
  }

  ctx.save();
  applyStrokeStyle(ctx, style);
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();

  ctx.setLineDash([]);
  ctx.beginPath();
  ctx.moveTo(x2, y2);
  ctx.lineTo(p1[0], p1[1]);
  ctx.lineTo(p2[0], p2[1]);
  ctx.closePath();
  ctx.fillStyle = style.stroke;
  ctx.fill();
  ctx.restore();
}

function renderFreehand(ctx: CanvasRenderingContext2D, el: ElementData): void {
  const d = el.data as { points?: number[][] };
  const points = d.points ?? [];
  if (points.length === 0) return;
  const style = el.style;

  const outline = getStroke(points, {
    size: Math.max(style.strokeWidth * 4, 4),
    thinning: 0.6,
    smoothing: 0.5,
    streamline: 0.5,
  });
  if (outline.length === 0) return;

  const path = new Path2D();
  path.moveTo(outline[0][0], outline[0][1]);
  for (let i = 1; i < outline.length; i++) {
    path.lineTo(outline[i][0], outline[i][1]);
  }
  path.closePath();

  ctx.save();
  ctx.fillStyle = style.stroke;
  ctx.fill(path);
  ctx.restore();
}

function renderTextBlock(
  ctx: CanvasRenderingContext2D,
  text: string,
  x: number,
  y: number,
  style: ElementStyle
): void {
  if (!text) return;
  ctx.save();
  ctx.font = `${style.fontSize}px ${style.fontFamily}`;
  ctx.fillStyle = style.stroke;
  ctx.textAlign = style.textAlign;
  ctx.textBaseline = "top";
  const lineHeight = style.fontSize * 1.25;
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    ctx.fillText(lines[i], x, y + i * lineHeight);
  }
  ctx.restore();
}

function renderText(ctx: CanvasRenderingContext2D, el: ElementData): void {
  const d = el.data as { x?: number; y?: number; text?: string };
  renderTextBlock(ctx, d.text ?? "", d.x ?? 0, d.y ?? 0, el.style);
}

function renderStickyText(ctx: CanvasRenderingContext2D, el: ElementData): void {
  const d = el.data as { x?: number; y?: number; width?: number; text?: string };
  const padding = 12;
  const x = (d.x ?? 0) + padding;
  const y = (d.y ?? 0) + padding;
  renderTextBlock(ctx, d.text ?? "", x, y, { ...el.style, stroke: "#1c1917", textAlign: "left" });
}

function renderImagePlaceholder(ctx: CanvasRenderingContext2D, el: ElementData): void {
  const d = el.data as { x?: number; y?: number; width?: number; height?: number };
  const x = d.x ?? 0;
  const y = d.y ?? 0;
  const width = d.width ?? 0;
  const height = d.height ?? 0;
  ctx.save();
  ctx.fillStyle = IMAGE_PLACEHOLDER_FILL;
  ctx.fillRect(x, y, width, height);
  ctx.strokeStyle = IMAGE_PLACEHOLDER_STROKE;
  ctx.lineWidth = 1;
  ctx.setLineDash([6, 4]);
  ctx.strokeRect(x, y, width, height);
  ctx.restore();
}

/**
 * Render a single element into a DPR/pan/zoom-transformed 2D context.
 * `mode` selects clean (native ctx paths) vs sketch (roughjs) rendering;
 * freehand and text render identically in both modes.
 */
export function renderElement(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode
): void {
  ctx.save();
  ctx.globalAlpha = el.style.opacity;

  switch (el.elementType) {
    case "rectangle":
    case "sticky":
      renderBox(ctx, rc, el, mode);
      if (el.elementType === "sticky") renderStickyText(ctx, el);
      break;
    case "ellipse":
      renderEllipse(ctx, rc, el, mode);
      break;
    case "line":
      renderLine(ctx, rc, el, mode);
      break;
    case "arrow":
      renderArrow(ctx, rc, el, mode);
      break;
    case "freehand":
      renderFreehand(ctx, el);
      break;
    case "text":
      renderText(ctx, el);
      break;
    case "image":
      renderImagePlaceholder(ctx, el);
      break;
    default:
      break;
  }

  ctx.restore();
}
