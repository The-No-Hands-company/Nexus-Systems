import { useGPUStore } from "../stores/useGPUStore";

function GpuChip({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="bg-zinc-800 rounded px-2 py-1">
      <div className="text-[10px] text-zinc-500 uppercase">{label}</div>
      <div className="text-xs text-zinc-200 font-mono truncate">{value}</div>
    </div>
  );
}

export default function Dashboard() {
  const workers = useGPUStore((s) => s.workers);

  return (
    <div className="space-y-2">
      <h2 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Workers</h2>
      {workers.length === 0 && (
        <div className="text-xs text-zinc-600 py-4 text-center">No workers registered</div>
      )}
      {workers.map((w) => (
        <div key={w.id} className="bg-zinc-800/50 rounded p-2 space-y-1 border border-zinc-800">
          <div className="flex items-center justify-between">
            <span className="text-sm text-zinc-200 font-medium truncate">{w.gpu_name}</span>
            <span className={`w-2 h-2 rounded-full shrink-0 ${w.online ? "bg-green-500" : "bg-red-500"}`} />
          </div>
          <div className="grid grid-cols-2 gap-1">
            <GpuChip label="VRAM" value={`${w.memory_mb} MB`} />
            <GpuChip label="Jobs" value={w.jobs_completed} />
          </div>
          <div className="flex gap-1 text-[10px] text-zinc-500">
            {w.capabilities.slice(0, 3).map((c) => (
              <span key={c} className="bg-zinc-700/50 px-1 rounded">{c}</span>
            ))}
          </div>
        </div>
      ))}
    </div>
  );
}
