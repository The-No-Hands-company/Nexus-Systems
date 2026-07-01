import { useGPUStore } from "../stores/useGPUStore";

export default function DiffViewer() {
  const job = useGPUStore((s) => s.selectedJob);
  const selectJob = useGPUStore((s) => s.selectJob);

  if (!job) return null;

  const metrics = job.result_metrics;
  const thresholdPct = metrics
    ? ((metrics.pixels_above_threshold / metrics.total_pixels) * 100).toFixed(2)
    : "—";

  return (
    <div className="space-y-4">
      <div className="flex items-center gap-3">
        <button onClick={() => selectJob(null)} className="text-xs text-zinc-500 hover:text-zinc-300">
          ← Back
        </button>
        <h2 className="text-lg font-semibold text-zinc-100">{job.name}</h2>
        <span className={`text-xs px-2 py-0.5 rounded ${
          job.status === "passed" ? "text-green-400 bg-green-900/30" :
          job.status === "failed" ? "text-red-400 bg-red-900/30" :
          "text-zinc-400 bg-zinc-800"
        }`}>{job.status}</span>
      </div>

      {metrics ? (
        <div className="grid grid-cols-4 gap-3">
          <MetricCard label="Mean Error" value={metrics.mean_error.toFixed(4)} />
          <MetricCard label="Max Error" value={metrics.max_error.toFixed(4)} />
          <MetricCard label="Pixels Above Threshold" value={`${metrics.pixels_above_threshold} / ${metrics.total_pixels}`} />
          <MetricCard label="Failed Pixels" value={`${thresholdPct}%`} />
          <MetricCard label="Duration" value={job.duration_ms ? `${job.duration_ms}ms` : "—"} />
          <MetricCard label="Viewport" value={`${job.viewport_width}×${job.viewport_height}`} />
          <MetricCard label="Mesh Type" value={job.mesh_type} />
          <MetricCard label="Tolerance" value={`${job.tolerance} JND`} />
        </div>
      ) : (
        <div className="text-sm text-zinc-500 py-8 text-center">
          {job.status === "pending" ? "Job is queued — waiting for a worker..." :
           job.status === "running" ? "Job is running on a worker..." :
           "No results available"}
        </div>
      )}

      {job.diff_image_key && (
        <div className="border border-zinc-800 rounded overflow-hidden">
          <div className="p-2 text-xs text-zinc-500 bg-zinc-900">Perceptual Diff Heatmap</div>
          <div className="bg-zinc-900 p-4 flex items-center justify-center">
            <img
              src={`/api/v1/gpu-test/assets/${job.diff_image_key}`}
              alt="Diff heatmap"
              className="max-w-full max-h-96"
            />
          </div>
        </div>
      )}
    </div>
  );
}

function MetricCard({ label, value }: { label: string; value: string }) {
  return (
    <div className="bg-zinc-800/50 border border-zinc-800 rounded p-3">
      <div className="text-[10px] text-zinc-500 uppercase tracking-wider mb-1">{label}</div>
      <div className="text-sm text-zinc-200 font-mono">{value}</div>
    </div>
  );
}
