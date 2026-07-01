import { useState } from "react";
import { useGPUStore } from "../stores/useGPUStore";

export default function JobSubmitter() {
  const submitJob = useGPUStore((s) => s.submitJob);
  const [name, setName] = useState("");
  const [meshType, setMeshType] = useState("triangle");
  const [width, setWidth] = useState(256);
  const [height, setHeight] = useState(256);
  const [tolerance, setTolerance] = useState(2.3);
  const [shaderSource, setShaderSource] = useState("");
  const [open, setOpen] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim()) return;
    await submitJob({
      name: name.trim(),
      mesh_type: meshType,
      viewport_width: width,
      viewport_height: height,
      tolerance,
      shader_source: shaderSource || undefined,
    });
    setName("");
    setShaderSource("");
  };

  if (!open) {
    return (
      <button onClick={() => setOpen(true)} className="w-full py-2 bg-blue-600 hover:bg-blue-500 text-white text-sm rounded transition-colors">
        + New Test Job
      </button>
    );
  }

  return (
    <form onSubmit={handleSubmit} className="space-y-2 bg-zinc-800/50 rounded p-3 border border-zinc-700">
      <div className="flex items-center justify-between">
        <h3 className="text-xs font-semibold text-zinc-300 uppercase">New Job</h3>
        <button type="button" onClick={() => setOpen(false)} className="text-xs text-zinc-500 hover:text-zinc-300">✕</button>
      </div>
      <input value={name} onChange={(e) => setName(e.target.value)} placeholder="Job name" className="w-full bg-zinc-800 px-2 py-1.5 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none" />
      <select value={meshType} onChange={(e) => setMeshType(e.target.value)} className="w-full bg-zinc-800 px-2 py-1.5 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none">
        <option value="triangle">Triangle</option>
        <option value="quad">Quad</option>
        <option value="mesh">Mesh Shader</option>
        <option value="compute">Compute Shader</option>
      </select>
      <div className="grid grid-cols-2 gap-2">
        <div>
          <label className="text-[10px] text-zinc-500">Width</label>
          <input type="number" value={width} onChange={(e) => setWidth(Number(e.target.value))} className="w-full bg-zinc-800 px-2 py-1 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </div>
        <div>
          <label className="text-[10px] text-zinc-500">Height</label>
          <input type="number" value={height} onChange={(e) => setHeight(Number(e.target.value))} className="w-full bg-zinc-800 px-2 py-1 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </div>
      </div>
      <div>
        <label className="text-[10px] text-zinc-500">Tolerance (JND)</label>
        <input type="number" step="0.1" value={tolerance} onChange={(e) => setTolerance(Number(e.target.value))} className="w-full bg-zinc-800 px-2 py-1 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none" />
      </div>
      <textarea value={shaderSource} onChange={(e) => setShaderSource(e.target.value)} placeholder="Optional: GLSL/Vulkan shader source" rows={3} className="w-full bg-zinc-800 px-2 py-1 text-sm rounded border border-zinc-700 focus:border-blue-500 outline-none font-mono text-[11px]" />
      <button type="submit" className="w-full py-1.5 bg-blue-600 hover:bg-blue-500 text-white text-sm rounded transition-colors">
        Submit
      </button>
    </form>
  );
}
