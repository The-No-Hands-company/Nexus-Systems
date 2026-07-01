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

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const glRef = useRef<WebGL2RenderingContext | null>(null);
  const programRef = useRef<WebGLProgram | null>(null);
  const dragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });

  const pan = useEditorStore((s) => s.pan);
  const zoom = useEditorStore((s) => s.zoom);
  const setPan = useEditorStore((s) => s.setPan);
  const setZoom = useEditorStore((s) => s.setZoom);
  const frames = useEditorStore((s) => s.frames);
  const selectedFrameId = useEditorStore((s) => s.selectedFrameId);
  const activeTool = useEditorStore((s) => s.activeTool);
  const selectLayer = useEditorStore((s) => s.selectLayer);
  const deselectAll = useEditorStore((s) => s.deselectAll);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);

  const drawGrid = useCallback(() => {
    const gl = glRef.current;
    if (!gl) return;
    gl.clear(gl.COLOR_BUFFER_BIT);
  }, []);

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

    const quad = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.useProgram(prog);
    const aPos = gl.getAttribLocation(prog, "a_pos");
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

    const resize = () => {
      canvas.width = canvas.clientWidth * devicePixelRatio;
      canvas.height = canvas.clientHeight * devicePixelRatio;
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.uniform2f(gl.getUniformLocation(prog, "u_resolution"), canvas.width, canvas.height);
      drawGrid();
    };
    resize();
    window.addEventListener("resize", resize);
    return () => { window.removeEventListener("resize", resize); gl.deleteProgram(prog); };
  }, [drawGrid]);

  useEffect(() => {
    const gl = glRef.current;
    if (!gl) return;
    const prog = programRef.current;
    if (!prog) return;
    gl.useProgram(prog);
    gl.uniform2f(gl.getUniformLocation(prog, "u_pan"), pan.x, pan.y);
    gl.uniform1f(gl.getUniformLocation(prog, "u_zoom"), zoom);
    gl.uniform4f(gl.getUniformLocation(prog, "u_color"), 0.15, 0.15, 0.2, 1.0);
    drawGrid();
  }, [pan, zoom, drawGrid]);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    const delta = -e.deltaY * 0.001;
    setZoom(zoom * (1 + delta));
  }, [zoom, setZoom]);

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    if (e.button === 1 || (e.button === 0 && activeTool === "hand")) {
      dragging.current = true;
      lastMouse.current = { x: e.clientX, y: e.clientY };
    }
    if (e.button === 0 && activeTool === "select") {
      deselectAll();
    }
  }, [activeTool, deselectAll]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (!dragging.current) return;
    const dx = e.clientX - lastMouse.current.x;
    const dy = e.clientY - lastMouse.current.y;
    setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
    lastMouse.current = { x: e.clientX, y: e.clientY };
  }, [pan, zoom, setPan]);

  const handleMouseUp = useCallback(() => { dragging.current = false; }, []);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    if (e.key === " " && !e.repeat) { setActiveTool("hand"); e.preventDefault(); }
    if (e.key === "v") { setActiveTool("select"); }
    if (e.key === "h") { setActiveTool("hand"); }
    if (e.key === "f") { setActiveTool("frame"); }
    if (e.key === "r") { setActiveTool("rectangle"); }
    if (e.key === "e") { setActiveTool("ellipse"); }
    if (e.key === "t") { setActiveTool("text"); }
    if (e.key === "c") { setActiveTool("component"); }
    if (e.key === "p") { setActiveTool("prototype"); }
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
