import { useEffect, useState } from "react";
import type { Project, Task, Workspace } from "../types";
import { useApi } from "../runtime";
import { PriorityIcon } from "../components/PriorityIcon";

export function HomeView({ onSelectProject }: { onSelectProject: (p: Project) => void }) {
  const api = useApi();
  const [workspaces, setWorkspaces] = useState<Workspace[]>([]);
  const [activeWs, setActiveWs] = useState<string | null>(null);
  const [projects, setProjects] = useState<Project[]>([]);
  const [myTasks, setMyTasks] = useState<Task[]>([]);
  const [tab, setTab] = useState<"projects" | "my-tasks">("projects");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    Promise.all([api.listWorkspaces(), api.myTasks()])
      .then(([ws, tasks]) => {
        if (cancelled) return;
        setWorkspaces(ws);
        setMyTasks(tasks);
        if (ws.length > 0) setActiveWs(ws[0].id);
        setLoading(false);
      })
      .catch(e => { if (!cancelled) { setError(String(e)); setLoading(false); } });
    return () => { cancelled = true; };
  }, []);

  useEffect(() => {
    if (!activeWs) return;
    api.listProjects(activeWs).then(setProjects).catch(() => setProjects([]));
  }, [activeWs]);

  if (loading) return <div className="p-8 text-zinc-400">Loading…</div>;
  if (error) return <div className="p-8 text-red-400">{error}</div>;

  return (
    <div className="flex flex-col h-full">
      {/* workspace switcher */}
      <div className="flex gap-2 px-6 pt-6 pb-2 flex-wrap">
        {workspaces.map(ws => (
          <button
            key={ws.id}
            onClick={() => setActiveWs(ws.id)}
            className={`px-3 py-1.5 rounded text-sm font-medium border transition-colors ${
              activeWs === ws.id
                ? "bg-zinc-700 border-zinc-500 text-zinc-100"
                : "bg-transparent border-zinc-700 text-zinc-400 hover:text-zinc-200"
            }`}
          >
            {ws.name}
            <span className="ml-1.5 text-xs text-zinc-500">{ws.kind}</span>
          </button>
        ))}
      </div>

      {/* tabs */}
      <div className="flex gap-0 px-6 border-b border-zinc-800 mt-2">
        {(["projects", "my-tasks"] as const).map(t => (
          <button
            key={t}
            onClick={() => setTab(t)}
            className={`px-4 py-2 text-sm border-b-2 transition-colors ${
              tab === t
                ? "border-zinc-100 text-zinc-100"
                : "border-transparent text-zinc-500 hover:text-zinc-300"
            }`}
          >
            {t === "projects" ? "Projects" : "My Tasks"}
          </button>
        ))}
      </div>

      <div className="flex-1 overflow-y-auto p-6">
        {tab === "projects" && (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {projects.length === 0 && (
              <div className="col-span-full text-zinc-500 text-sm">No projects in this workspace.</div>
            )}
            {projects.map(p => (
              <button
                key={p.id}
                onClick={() => onSelectProject(p)}
                className="text-left bg-zinc-900 border border-zinc-700 rounded-lg p-4 hover:border-zinc-500 transition-colors"
              >
                <div className="flex items-start justify-between gap-2">
                  <span className="font-semibold text-zinc-100">{p.name}</span>
                  <span className="text-xs font-mono text-zinc-500 bg-zinc-800 px-1.5 py-0.5 rounded">{p.key}</span>
                </div>
                {p.description && <p className="text-sm text-zinc-400 mt-1 line-clamp-2">{p.description}</p>}
                <div className="mt-3 flex gap-3 text-xs text-zinc-500">
                  <span>{p.taskCount} tasks</span>
                  {p.startDate && <span>Started {p.startDate}</span>}
                  <span className="ml-auto capitalize">{p.scheduleMode}</span>
                </div>
              </button>
            ))}
          </div>
        )}

        {tab === "my-tasks" && (
          <div className="space-y-1">
            {myTasks.length === 0 && (
              <div className="text-zinc-500 text-sm">No tasks assigned to you.</div>
            )}
            {myTasks.map(t => (
              <div key={t.id} className="flex items-center gap-3 px-3 py-2 rounded hover:bg-zinc-800 text-sm">
                <PriorityIcon priority={t.priority} />
                <span className="flex-1 text-zinc-200">{t.title}</span>
                <span className="text-xs text-zinc-500 font-mono">#{t.number}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
