import { useEffect, useRef, useCallback } from "react";
import { useEditorStore } from "../../stores/useEditorStore";

const VS_SRC = `#version 300 es
in vec2 a_pos;
uniform vec2 u_resolution;
uniform vec2 u_pan;
uniform float u_zoom;
void main() {
  vec2 pos = (a_pos + u_pan) * u_zoom;
  vec2 clip = pos / u_resolution * 2.0 - 1.0;
  gl_Position = vec4(clip, 0.0, 1.0);
}`;

const FS_SRC = `#version 300 es
precision highp float;
uniform vec4 u_color;
out vec4 fragColor;
void main() { fragColor = u_color; }`;

const GRID_SIZE = 40;

function hexToRgba(hex: string, alpha: number = 1): [number, number, number, number] {
  const h = hex.replace("#", "");
  const r = parseInt(h.substring(0, 2), 16) / 255;
  const g = parseInt(h.substring(2, 4), 16) / 255;
  const b = parseInt(h.substring(4, 6), 16) / 255;
  return [r, g, b, alpha];
}

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const glRef = useRef<WebGL2RenderingContext | null>(null);
  const programRef = useRef<WebGLProgram | null>(null);
  const vaoRef = useRef<WebGLVertexArrayObject | null>(null);
  const dragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
  const drawStart = useRef({ x: 0, y: 0 });

  const pan = useEditorStore((s) => s.pan);
  const zoom = useEditorStore((s) => s.zoom);
  const setPan = useEditorStore((s) => s.setPan);
  const setZoom = useEditorStore((s) => s.setZoom);
  const elements = useEditorStore((s) => s.elements);
  const activeTool = useEditorStore((s) => s.activeTool);
  const selectElement = useEditorStore((s) => s.selectElement);
  const deselectAll = useEditorStore((s) => s.deselectAll);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);
  const addElement = useEditorStore((s) => s.addElement);
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);

  const screenToWorld = useCallback((cx: number, cy: number) => {
    return { x: cx / zoom - pan.x, y: cy / zoom - pan.y };
  }, [pan, zoom]);

  const drawElements = useCallback(() => {
    const gl = glRef.current;
    const prog = programRef.current;
    if (!gl || !prog) return;

    gl.useProgram(prog);
    const uColor = gl.getUniformLocation(prog, "u_color");

    for (const el of elements) {
      const data = el.data as Record<string, any>;
      const style = el.style as Record<string, any>;
      const opacity = (style.opacity as number) ?? 1;
      const fill = (style.fill as string) ?? "#3b82f6";
      const stroke = (style.stroke as string) ?? "#000000";
      const strokeWidth = (style.strokeWidth as number) ?? 2;
      const x = (data.x as number) ?? 0;
      const y = (data.y as number) ?? 0;
      const w = (data.width as number) ?? 100;
      const h = (data.height as number) ?? 100;
      const isSelected = selectedElementIds.has(el.id);

      const drawRect = (rx: number, ry: number, rw: number, rh: number, color: [number, number, number, number]) => {
        const verts = new Float32Array([
          rx, ry, rx + rw, ry, rx, ry + rh,
          rx + rw, ry, rx + rw, ry + rh, rx, ry + rh,
        ]);
        const buf = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buf);
        gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);
        const aPos = gl.getAttribLocation(prog, "a_pos");
        gl.enableVertexAttribArray(aPos);
        gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);
        gl.uniform4f(uColor, color[0], color[1], color[2], color[3]);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.deleteBuffer(buf);
      };

      const drawEllipse = (cx: number, cy: number, rx: number, ry: number, color: [number, number, number, number]) => {
        const segments = 32;
        const verts = new Float32Array((segments + 2) * 2);
        verts[0] = cx; verts[1] = cy;
        for (let i = 0; i <= segments; i++) {
          const angle = (i / segments) * Math.PI * 2;
          verts[(i + 1) * 2] = cx + Math.cos(angle) * rx;
          verts[(i + 1) * 2 + 1] = cy + Math.sin(angle) * ry;
        }
        const buf = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buf);
        gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);
        const aPos = gl.getAttribLocation(prog, "a_pos");
        gl.enableVertexAttribArray(aPos);
        gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);
        gl.uniform4f(uColor, color[0], color[1], color[2], color[3]);
        gl.drawArrays(gl.TRIANGLE_FAN, 0, segments + 2);
        gl.deleteBuffer(buf);
      };

      const elType = el.elementType as string;
      if (elType === "rectangle" || elType === "shape") {
        drawRect(x, y, w, h, hexToRgba(fill, opacity));
        drawRect(x, y, w, strokeWidth, hexToRgba(stroke, opacity));
        drawRect(x, y + h - strokeWidth, w, strokeWidth, hexToRgba(stroke, opacity));
        drawRect(x, y, strokeWidth, h, hexToRgba(stroke, opacity));
        drawRect(x + w - strokeWidth, y, strokeWidth, h, hexToRgba(stroke, opacity));
        if (isSelected) {
          drawRect(x - 2, y - 2, w + 4, 2, [0.3, 0.6, 1, 1]);
          drawRect(x - 2, y + h, w + 4, 2, [0.3, 0.6, 1, 1]);
          drawRect(x - 2, y - 2, 2, h + 4, [0.3, 0.6, 1, 1]);
          drawRect(x + w, y - 2, 2, h + 4, [0.3, 0.6, 1, 1]);
        }
      } else if (elType === "ellipse") {
        drawEllipse(x + w / 2, y + h / 2, w / 2, h / 2, hexToRgba(fill, opacity));
        if (isSelected) {
          drawRect(x - 2, y - 2, w + 4, 2, [0.3, 0.6, 1, 1]);
          drawRect(x - 2, y + h, w + 4, 2, [0.3, 0.6, 1, 1]);
          drawRect(x - 2, y - 2, 2, h + 4, [0.3, 0.6, 1, 1]);
          drawRect(x + w, y - 2, 2, h + 4, [0.3, 0.6, 1, 1]);
        }
      } else if (el.elementType === "sticky") {
        drawRect(x, y, w, h, hexToRgba("#ffd700", opacity));
        drawRect(x, y, w, h, hexToRgba("#d4a800", opacity));
      }
    }
  }, [elements, selectedElementIds]);

  const drawGrid = useCallback(() => {
    const gl = glRef.current;
    if (!gl) return;
    gl.clear(gl.COLOR_BUFFER_BIT);
    drawElements();
  }, [drawElements]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const gl = canvas.getContext("webgl2", { alpha: true, antialias: true });
    if (!gl) return;
    glRef.current = gl;

    const vs = gl.createShader(gl.VERTEX_SHADER)!;
    gl.shaderSource(vs, VS_SRC);
    gl.compileShader(vs);
    const fs = gl.createShader(gl.FRAGMENT_SHADER)!;
    gl.shaderSource(fs, FS_SRC);
    gl.compileShader(fs);
    const prog = gl.createProgram()!;
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    programRef.current = prog;

    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    vaoRef.current = vao;

    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    const resize = () => {
      canvas.width = canvas.clientWidth * devicePixelRatio;
      canvas.height = canvas.clientHeight * devicePixelRatio;
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.useProgram(prog);
      gl.uniform2f(gl.getUniformLocation(prog, "u_resolution"), canvas.width, canvas.height);
      drawGrid();
    };
    resize();
    window.addEventListener("resize", resize);
    return () => { window.removeEventListener("resize", resize); gl.deleteProgram(prog); };
  }, [drawGrid]);

  useEffect(() => {
    const gl = glRef.current;
    const prog = programRef.current;
    if (!gl || !prog) return;
    gl.useProgram(prog);
    gl.uniform2f(gl.getUniformLocation(prog, "u_pan"), pan.x, pan.y);
    gl.uniform1f(gl.getUniformLocation(prog, "u_zoom"), zoom);
    drawGrid();
  }, [pan, zoom, elements, selectedElementIds, drawGrid]);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    const delta = -e.deltaY * 0.001;
    setZoom(zoom * (1 + delta));
  }, [zoom, setZoom]);

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    const rect = canvasRef.current!.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const world = screenToWorld(mx, my);

    if (e.button === 1 || (e.button === 0 && activeTool === "hand")) {
      dragging.current = true;
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }

    if (e.button === 0) {
      if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
        dragging.current = true;
        drawStart.current = { x: world.x, y: world.y };
        lastMouse.current = { x: world.x, y: world.y };
      } else if (activeTool === "select") {
        const hit = [...elements].reverse().find((el) => {
          const d = el.data as Record<string, any>;
          const ex = (d.x as number) ?? 0;
          const ey = (d.y as number) ?? 0;
          const ew = (d.width as number) ?? 100;
          const eh = (d.height as number) ?? 100;
          return world.x >= ex && world.x <= ex + ew && world.y >= ey && world.y <= ey + eh;
        });
        if (hit) {
          selectElement(hit.id, e.metaKey || e.ctrlKey);
        } else {
          deselectAll();
        }
      }
    }
  }, [activeTool, screenToWorld, elements, selectElement, deselectAll]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (!dragging.current) return;

    if (activeTool === "hand") {
      const dx = e.clientX - lastMouse.current.x;
      const dy = e.clientY - lastMouse.current.y;
      setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }

    const rect = canvasRef.current!.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const world = screenToWorld(mx, my);

    if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
      const sx = drawStart.current.x;
      const sy = drawStart.current.y;
      const ex = world.x;
      const ey = world.y;
      const x = Math.min(sx, ex);
      const y = Math.min(sy, ey);
      const w = Math.abs(ex - sx);
      const h = Math.abs(ey - sy);

      const id = "__preview__";
      const existing = elements.filter((el) => el.id !== id);
      const preview = {
        id,
        elementType: (activeTool === "rectangle" ? "rectangle" : activeTool === "ellipse" ? "ellipse" : "line") as any,
        data: { x, y, width: w || 1, height: h || 1 },
        style: { fill: "#3b82f6", stroke: "#1d4ed8", strokeWidth: 2, opacity: 1 },
        transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
        order: existing.length,
      };
    }
  }, [activeTool, pan, zoom, setPan, screenToWorld, elements]);

  const handleMouseUp = useCallback((e: React.MouseEvent) => {
    if (!dragging.current) return;
    dragging.current = false;

    if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
      const rect = canvasRef.current!.getBoundingClientRect();
      const mx = e.clientX - rect.left;
      const my = e.clientY - rect.top;
      const world = screenToWorld(mx, my);

      const sx = drawStart.current.x;
      const sy = drawStart.current.y;
      const ex = world.x;
      const ey = world.y;
      const x = Math.min(sx, ex);
      const y = Math.min(sy, ey);
      const w = Math.abs(ex - sx);
      const h = Math.abs(ey - sy);

      if (w > 2 || h > 2) {
        addElement({
          id: crypto.randomUUID(),
          elementType: (activeTool === "rectangle" ? "rectangle" : activeTool === "ellipse" ? "ellipse" : "arrow") as any,
          data: { x, y, width: w, height: h },
          style: { fill: "#3b82f6", stroke: "#1d4ed8", strokeWidth: 2, opacity: 1 },
          transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
          order: elements.length,
        });
      }
    }
  }, [activeTool, screenToWorld, addElement, elements]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    if (e.key === " " && !e.repeat) { setActiveTool("hand"); e.preventDefault(); }
    if (e.key === "v") { setActiveTool("select"); }
    if (e.key === "p") { setActiveTool("pen"); }
    if (e.key === "r") { setActiveTool("rectangle"); }
    if (e.key === "e") { setActiveTool("ellipse"); }
    if (e.key === "l") { setActiveTool("line"); }
    if (e.key === "a") { setActiveTool("arrow"); }
    if (e.key === "t") { setActiveTool("text"); }
    if (e.key === "s") { setActiveTool("sticky"); }
    if (e.key === "u" && (e.metaKey || e.ctrlKey) && !e.shiftKey) { e.preventDefault(); useEditorStore.getState().undo(); }
    if (e.key === "z" && (e.metaKey || e.ctrlKey) && e.shiftKey) { e.preventDefault(); useEditorStore.getState().redo(); }
    if ((e.metaKey || e.ctrlKey) && e.key === "0") { setZoom(1); setPan({ x: 0, y: 0 }); }
  }, [setActiveTool, setZoom, setPan]);

  const handleKeyUp = useCallback((e: KeyboardEvent) => {
    if (e.key === " " && activeTool === "hand") {
      setActiveTool("select");
    }
  }, [activeTool, setActiveTool]);

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    return () => { window.removeEventListener("keydown", handleKeyDown); window.removeEventListener("keyup", handleKeyUp); };
  }, [handleKeyDown, handleKeyUp]);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-full cursor-crosshair"
      style={{ background: "#1a1a2e" }}
      onWheel={handleWheel}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
    />
  );
}
