import { useGPUStore } from "../stores/useGPUStore";

function statusColor(status: string) {
  switch (status) {
    case "passed": return "text-green-400 bg-green-900/30";
    case "failed": return "text-red-400 bg-red-900/30";
    case "running": return "text-blue-400 bg-blue-900/30";
    case "pending": return "text-yellow-400 bg-yellow-900/30";
    default: return "text-zinc-400 bg-zinc-800";
  }
}

function timeAgo(dateStr: string): string {
  const ms = Date.now() - new Date(dateStr).getTime();
  if (ms < 60000) return "just now";
  if (ms < 3600000) return `${Math.floor(ms / 60000)}m ago`;
  if (ms < 86400000) return `${Math.floor(ms / 3600000)}h ago`;
  return `${Math.floor(ms / 86400000)}d ago`;
}

export default function JobHistory() {
  const jobs = useGPUStore((s) => s.jobs);
  const selectJob = useGPUStore((s) => s.selectJob);
  const loading = useGPUStore((s) => s.loading);

  if (loading && jobs.length === 0) {
    return (
      <div className="flex items-center justify-center h-48 text-sm text-zinc-500">
        Loading...
      </div>
    );
  }

  if (jobs.length === 0) {
    return (
      <div className="flex items-center justify-center h-48 text-sm text-zinc-500">
        No test jobs yet. Create one to get started.
      </div>
    );
  }

  return (
    <div className="space-y-2">
      <h2 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider mb-3">
        Test Jobs ({jobs.length})
      </h2>
      <div className="grid gap-2">
        {jobs.map((job) => (
          <button
            key={job.id}
            onClick={() => selectJob(job)}
            className="flex items-center gap-3 w-full text-left bg-zinc-800/30 hover:bg-zinc-800/60 rounded p-3 border border-zinc-800 transition-colors"
          >
            <div className="flex-1 min-w-0">
              <div className="text-sm text-zinc-200 font-medium truncate">{job.name}</div>
              <div className="text-[11px] text-zinc-500 mt-0.5">
                {job.mesh_type} · {job.viewport_width}×{job.viewport_height}
              </div>
            </div>
            <div className="text-right">
              <span className={`text-xs px-2 py-0.5 rounded ${statusColor(job.status)}`}>
                {job.status}
              </span>
              {job.duration_ms && (
                <div className="text-[10px] text-zinc-600 mt-0.5">{job.duration_ms}ms</div>
              )}
            </div>
            <div className="text-[10px] text-zinc-600 w-12 text-right">
              {timeAgo(job.created_at)}
            </div>
          </button>
        ))}
      </div>
    </div>
  );
}
