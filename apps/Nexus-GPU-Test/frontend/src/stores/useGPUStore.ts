import { create } from "zustand";

export interface Worker {
  id: string;
  gpu_name: string;
  driver_version: string;
  vulkan_version: string;
  memory_mb: number;
  capabilities: string[];
  online: boolean;
  last_heartbeat: string | null;
  jobs_completed: number;
}

export interface TestJob {
  id: string;
  name: string;
  status: "pending" | "running" | "passed" | "failed" | "error";
  mesh_type: string;
  viewport_width: number;
  viewport_height: number;
  tolerance: number;
  worker_id: string | null;
  result_metrics: { mean_error: number; max_error: number; pixels_above_threshold: number; total_pixels: number } | null;
  duration_ms: number | null;
  diff_image_key: string | null;
  rendered_image_key: string | null;
  created_at: string;
}

interface GPUStore {
  workers: Worker[];
  jobs: TestJob[];
  selectedJob: TestJob | null;
  loading: boolean;

  fetchWorkers: () => Promise<void>;
  fetchJobs: () => Promise<void>;
  selectJob: (job: TestJob | null) => void;
  submitJob: (body: { name: string; mesh_type: string; viewport_width: number; viewport_height: number; tolerance: number; shader_source?: string }) => Promise<void>;
}

const API = "/api/v1/gpu-test";

export const useGPUStore = create<GPUStore>((set) => ({
  workers: [],
  jobs: [],
  selectedJob: null,
  loading: false,

  fetchWorkers: async () => {
    try {
      const res = await fetch(`${API}/workers`);
      if (res.ok) set({ workers: await res.json() });
    } catch { /* ignore */ }
  },

  fetchJobs: async () => {
    set({ loading: true });
    try {
      const res = await fetch(`${API}/jobs`);
      if (res.ok) set({ jobs: await res.json() });
    } catch { /* ignore */ }
    set({ loading: false });
  },

  selectJob: (job) => set({ selectedJob: job }),

  submitJob: async (body) => {
    const res = await fetch(`${API}/jobs`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    });
    if (res.ok) {
      const job = await res.json();
      set((s) => ({ jobs: [job, ...s.jobs] }));
    }
  },
}));
