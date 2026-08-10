import rough from "roughjs";
import { getStroke } from "perfect-freehand";
import type { ElementData, StyleMode } from "../stores/model";
import { arrowHead } from "./geometry";

export type RoughCanvasSvg = ReturnType<typeof rough.canvas>;

const imageCache = new Map<string, HTMLImageElement>();

function loadImage(src: string): HTMLImageElement | null {
  const cached = imageCache.get(src);
  if (cached) return cached;
  const img = new Image();
  img.src = src;
  imageCache.set(src, img);
  return img;
}

function sketchOpts(el: ElementData) {
  const s = el.style;
  return {
    seed: el.seed,
    roughness: 1.3,
    bowing: 1,
    stroke: s.stroke,
    strokeWidth: s.strokeWidth,
    fill: s.fill === "none" ? undefined : s.fill,
    fillStyle: "hachure" as const,
  };
}

function drawDashed(ctx: CanvasRenderingContext2D, style: { strokeStyle: string; strokeWidth: number }) {
  if (style.strokeStyle === "dashed") ctx.setLineDash([6, 6]);
  else if (style.strokeStyle === "dotted") ctx.setLineDash([1.5, 4]);
  else ctx.setLineDash([]);
}

export function renderElement(ctx: CanvasRenderingContext2D, rc: RoughCanvasSvg, el: ElementData, mode: StyleMode): void {
  if (el.data.hidden) return;
  const s = el.style;
  ctx.save();
  ctx.globalAlpha = s.opacity;

  const b = el.transform;
  if (b.a !== 1 || b.b !== 0 || b.c !== 0 || b.d !== 1 || b.e !== 0 || b.f !== 0) {
    ctx.transform(b.a, b.b, b.c, b.d, b.e, b.f);
  }

  switch (el.elementType) {
    case "rectangle": {
      const { x, y, width, height } = el.data as { x: number; y: number; width: number; height: number };
      if (mode === "sketch") {
        rc.rectangle(x, y, width, height, sketchOpts(el));
      } else {
        ctx.strokeStyle = s.stroke;
        ctx.lineWidth = s.strokeWidth;
        drawDashed(ctx, s);
        if (s.fill && s.fill !== "none") {
          ctx.fillStyle = s.fill;
          ctx.beginPath();
          ctx.roundRect(x, y, width, height, s.radius);
          ctx.fill();
        }
        ctx.beginPath();
        ctx.roundRect(x, y, width, height, s.radius);
        ctx.stroke();
      }
      break;
    }
    case "ellipse": {
      const { x, y, width, height } = el.data as { x: number; y: number; width: number; height: number };
      if (mode === "sketch") {
        rc.ellipse(x + width / 2, y + height / 2, width, height, sketchOpts(el));
      } else {
        ctx.strokeStyle = s.stroke;
        ctx.lineWidth = s.strokeWidth;
        drawDashed(ctx, s);
        if (s.fill && s.fill !== "none") {
          ctx.fillStyle = s.fill;
          ctx.beginPath();
          ctx.ellipse(x + width / 2, y + height / 2, width / 2, height / 2, 0, 0, Math.PI * 2);
          ctx.fill();
        }
        ctx.beginPath();
        ctx.ellipse(x + width / 2, y + height / 2, width / 2, height / 2, 0, 0, Math.PI * 2);
        ctx.stroke();
      }
      break;
    }
    case "line":
    case "arrow": {
      const { x1, y1, x2, y2 } = el.data as { x1: number; y1: number; x2: number; y2: number };
      if (mode === "sketch") {
        rc.line(x1, y1, x2, y2, sketchOpts(el));
      } else {
        ctx.strokeStyle = s.stroke;
        ctx.lineWidth = s.strokeWidth;
        drawDashed(ctx, s);
        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.stroke();
      }
      if (el.elementType === "arrow") {
        const { p1, p2 } = arrowHead(x1, y1, x2, y2, 12 + el.style.strokeWidth * 2);
        ctx.fillStyle = s.stroke;
        ctx.strokeStyle = s.stroke;
        ctx.beginPath();
        ctx.moveTo(x2, y2);
        ctx.lineTo(p1[0], p1[1]);
        ctx.lineTo(p2[0], p2[1]);
        ctx.closePath();
        if (mode === "sketch") ctx.stroke(); else ctx.fill();
      }
      break;
    }
    case "freehand": {
      const pts = (el.data.points as { x: number; y: number; pressure?: number }[]).map((p) => [p.x, p.y, p.pressure ?? 0.5]);
      const outline = getStroke(pts, {
        size: s.strokeWidth * 3,
        thinning: 0.6,
        smoothing: 0.5,
        streamline: 0.5,
        simulatePressure: false,
      });
      const path = new Path2D();
      if (outline.length > 0) {
        path.moveTo(outline[0][0], outline[0][1]);
        for (const [x, y] of outline) path.lineTo(x, y);
        path.closePath();
        ctx.fillStyle = s.stroke;
        ctx.fill(path);
      }
      break;
    }
    case "text": {
      const { x, y, text } = el.data as { x: number; y: number; text?: string };
      ctx.fillStyle = s.stroke;
      ctx.font = `${s.fontSize}px ${s.fontFamily}`;
      ctx.textAlign = s.textAlign;
      ctx.textBaseline = "top";
      ctx.fillText(text ?? "Text", x, y);
      break;
    }
    case "sticky": {
      const { x, y, width, height, text } = el.data as { x: number; y: number; width: number; height: number; text?: string };
      if (mode === "sketch") {
        rc.rectangle(x, y, width, height, { ...sketchOpts(el), fill: "#fde047", fillStyle: "solid", stroke: "#ca8a04", roughness: 1.2 });
      } else {
        ctx.fillStyle = "#fde047";
        ctx.strokeStyle = "#ca8a04";
        ctx.lineWidth = s.strokeWidth;
        ctx.beginPath();
        ctx.roundRect(x, y, width, height, 4);
        ctx.fill();
        ctx.stroke();
      }
      ctx.fillStyle = "#713f12";
      ctx.font = `${s.fontSize}px ${s.fontFamily}`;
      ctx.textAlign = "left";
      ctx.textBaseline = "top";
      ctx.fillText(text ?? "", x + 8, y + 8, width - 16);
      break;
    }
    case "image": {
      const { x, y, width, height, src } = el.data as { x: number; y: number; width: number; height: number; src?: string };
      const img = src ? loadImage(src) : null;
      if (img) {
        ctx.drawImage(img, x, y, width, height);
        ctx.strokeStyle = s.stroke;
        ctx.lineWidth = s.strokeWidth;
        ctx.strokeRect(x, y, width, height);
      }
      break;
    }
  }

  ctx.restore();
}