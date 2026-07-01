import { useEffect } from "react";
import { useGPUStore } from "./stores/useGPUStore";
import Dashboard from "./components/Dashboard";
import JobSubmitter from "./components/JobSubmitter";
import JobHistory from "./components/JobHistory";
import DiffViewer from "./components/DiffViewer";

export default function App() {
  const selectedJob = useGPUStore((s) => s.selectedJob);
  const fetchWorkers = useGPUStore((s) => s.fetchWorkers);
  const fetchJobs = useGPUStore((s) => s.fetchJobs);

  useEffect(() => {
    fetchWorkers();
    fetchJobs();
    const timer = setInterval(() => { fetchWorkers(); fetchJobs(); }, 10000);
    return () => clearInterval(timer);
  }, [fetchWorkers, fetchJobs]);

  return (
    <div className="h-screen flex flex-col">
      <header className="h-12 flex items-center justify-between px-4 bg-zinc-900 border-b border-zinc-800 shrink-0">
        <div className="flex items-center gap-3">
          <span className="font-bold text-sm text-zinc-100">Nexus-GPU-Test</span>
          <span className="text-xs text-zinc-500">Federated Graphics Validation</span>
        </div>
        <div className="text-xs text-zinc-500">FLIP Perceptual Diff Engine v0.1</div>
      </header>

      <div className="flex-1 grid grid-cols-4 gap-0 overflow-hidden">
        <div className="col-span-1 border-r border-zinc-800 bg-zinc-900 overflow-y-auto p-4 space-y-4">
          <JobSubmitter />
          <Dashboard />
        </div>
        <div className="col-span-3 flex flex-col overflow-hidden">
          <div className="flex-1 overflow-y-auto p-4">
            {selectedJob ? (
              <DiffViewer />
            ) : (
              <JobHistory />
            )}
          </div>
        </div>
      </div>

      <footer className="h-8 flex items-center justify-between px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-600 shrink-0">
        <span>Workers: {useGPUStore.getState().workers.filter((w) => w.online).length} online</span>
        <span>Jobs: {useGPUStore.getState().jobs.length} total</span>
      </footer>
    </div>
  );
}
