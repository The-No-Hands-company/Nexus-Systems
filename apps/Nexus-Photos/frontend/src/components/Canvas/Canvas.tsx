import { useEffect, useRef, useCallback } from "react";
import { useEditorStore } from "../../stores/useEditorStore";

const VS_SRC = `#version 300 es
in vec2 a_pos;
uniform vec2 u_resolution;
void main() {
  vec2 clip = a_pos / u_resolution * 2.0 - 1.0;
  gl_Position = vec4(clip, 0.0, 1.0);
}`;

const FS_SRC = `#version 300 es
precision highp float;
uniform sampler2D u_texture;
uniform float u_brightness;
uniform float u_contrast;
uniform float u_saturation;
in vec2 v_texcoord;
out vec4 fragColor;

vec3 applyBrightness(vec3 c, float b) { return c + b; }
vec3 applyContrast(vec3 c, float v) {
  float t = (1.0 + v) / (1.0 - v + 0.001);
  return t * (c - 0.5) + 0.5;
}
vec3 applySaturation(vec3 c, float s) {
  float gray = dot(c, vec3(0.299, 0.587, 0.114));
  return mix(vec3(gray), c, 1.0 + s);
}

void main() {
  vec4 tex = texture(u_texture, v_texcoord);
  vec3 col = tex.rgb;
  col = applyBrightness(col, u_brightness);
  col = applyContrast(col, u_contrast);
  col = applySaturation(col, u_saturation);
  fragColor = vec4(col, tex.a);
}`;

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const glRef = useRef<WebGL2RenderingContext | null>(null);
  const programRef = useRef<WebGLProgram | null>(null);
  const textureRef = useRef<WebGLTexture | null>(null);

  const viewingPhoto = useEditorStore((s) => s.viewingPhoto);
  const filters = useEditorStore((s) => s.filters);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const gl = canvas.getContext("webgl2", { alpha: false, antialias: true, premultipliedAlpha: false });
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

    const quad = new Float32Array([
      -1, -1, 0, 0,
       1, -1, 1, 0,
      -1,  1, 0, 1,
       1,  1, 1, 1,
    ]);
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.useProgram(prog);

    const aPos = gl.getAttribLocation(prog, "a_pos");
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 16, 0);

    const aTex = gl.getAttribLocation(prog, "v_texcoord");
    gl.enableVertexAttribArray(aTex);
    gl.vertexAttribPointer(aTex, 2, gl.FLOAT, false, 16, 8);

    const tex = gl.createTexture();
    textureRef.current = tex;
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    const resize = () => {
      canvas.width = canvas.clientWidth * devicePixelRatio;
      canvas.height = canvas.clientHeight * devicePixelRatio;
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.uniform2f(gl.getUniformLocation(prog, "u_resolution"), canvas.width, canvas.height);
    };
    resize();
    window.addEventListener("resize", resize);
    return () => { window.removeEventListener("resize", resize); gl.deleteProgram(prog); };
  }, []);

  useEffect(() => {
    const gl = glRef.current;
    const prog = programRef.current;
    const tex = textureRef.current;
    if (!gl || !prog || !tex || !viewingPhoto) return;

    const img = new Image();
    img.crossOrigin = "anonymous";
    img.onload = () => {
      gl.useProgram(prog);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
      gl.uniform1i(gl.getUniformLocation(prog, "u_texture"), 0);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    };
    img.src = viewingPhoto.url;
  }, [viewingPhoto]);

  useEffect(() => {
    const gl = glRef.current;
    const prog = programRef.current;
    if (!gl || !prog) return;
    gl.useProgram(prog);
    gl.uniform1f(gl.getUniformLocation(prog, "u_brightness"), filters.brightness);
    gl.uniform1f(gl.getUniformLocation(prog, "u_contrast"), filters.contrast);
    gl.uniform1f(gl.getUniformLocation(prog, "u_saturation"), filters.saturation);
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
  }, [filters]);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-full"
      style={{ background: "#0a0a0f" }}
    />
  );
}
