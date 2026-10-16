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

  // create-workspace form
  const [creatingWs, setCreatingWs] = useState(false);
  const [wsName, setWsName] = useState("");
  const [wsKind, setWsKind] = useState<"personal" | "team">("team");
  const [wsLoading, setWsLoading] = useState(false);

  // create-project form
  const [creatingProject, setCreatingProject] = useState(false);
  const [projName, setProjName] = useState("");
  const [projKey, setProjKey] = useState("");
  const [projLoading, setProjLoading] = useState(false);

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

  // auto-derive project key from name
  useEffect(() => {
    const derived = projName.trim().toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 6);
    setProjKey(derived || "");
  }, [projName]);

  const submitCreateWs = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!wsName.trim() || wsLoading) return;
    setWsLoading(true);
    try {
      const ws = await api.createWorkspace(wsName.trim(), wsKind);
      setWorkspaces(prev => [...prev, ws]);
      setActiveWs(ws.id);
      setCreatingWs(false);
      setWsName("");
    } catch (err) {
      setError(String(err));
    } finally {
      setWsLoading(false);
    }
  };

  const submitCreateProject = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!projName.trim() || !projKey || !activeWs || projLoading) return;
    setProjLoading(true);
    try {
      const p = await api.createProject(activeWs, { name: projName.trim(), key: projKey });
      setProjects(prev => [...prev, p]);
      setCreatingProject(false);
      setProjName("");
    } catch (err) {
      setError(String(err));
    } finally {
      setProjLoading(false);
    }
  };

  if (loading) return <div className="p-8 text-zinc-500">Loading…</div>;
  if (error) return <div className="p-8 text-state-danger">{error}</div>;

  // No workspaces — onboarding
  if (workspaces.length === 0 && !creatingWs) {
    return (
      <div className="flex flex-col items-center justify-center h-full gap-5 text-center px-6">
        <svg width="44" height="44" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" className="text-zinc-700">
          <rect x="2" y="7" width="20" height="15" rx="2" />
          <path d="M16 7V5a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v2" />
          <line x1="12" y1="12" x2="12" y2="16" />
          <line x1="10" y1="14" x2="14" y2="14" />
        </svg>
        <div>
          <h2 className="text-xl font-semibold text-zinc-100">No workspaces yet</h2>
          <p className="mt-1.5 text-sm text-zinc-500 max-w-xs">
            Create a workspace to start organising your projects and tasks.
          </p>
        </div>
        <button
          onClick={() => setCreatingWs(true)}
          className="px-5 py-2.5 rounded-md bg-accent text-zinc-900 font-semibold text-sm hover:opacity-90 transition-opacity"
        >
          Create workspace
        </button>
      </div>
    );
  }

  // Create-workspace form (full-page overlay)
  if (creatingWs) {
    return (
      <div className="flex items-center justify-center h-full px-6">
        <form onSubmit={submitCreateWs} className="w-full max-w-sm flex flex-col gap-4 bg-zinc-800 border border-zinc-700 rounded-xl p-6">
          <h2 className="font-semibold text-zinc-100">New workspace</h2>
          <div className="flex flex-col gap-1.5">
            <label htmlFor="ws-name" className="text-xs text-zinc-500">Name</label>
            <input
              id="ws-name"
              autoFocus
              value={wsName}
              onChange={e => setWsName(e.target.value)}
              placeholder="My workspace"
              required
              className="bg-zinc-900 border border-zinc-600 rounded-md px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
            />
          </div>
          <div className="flex flex-col gap-1.5">
            <label htmlFor="ws-kind" className="text-xs text-zinc-500">Type</label>
            <select
              id="ws-kind"
              value={wsKind}
              onChange={e => setWsKind(e.target.value as "personal" | "team")}
              className="bg-zinc-900 border border-zinc-600 rounded-md px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
            >
              <option value="team">Team</option>
              <option value="personal">Personal</option>
            </select>
          </div>
          <div className="flex gap-2 pt-1">
            <button
              type="submit"
              disabled={!wsName.trim() || wsLoading}
              className="flex-1 py-2 rounded-md bg-accent text-zinc-900 font-semibold text-sm hover:opacity-90 disabled:opacity-50 transition-opacity"
            >
              {wsLoading ? "Creating…" : "Create"}
            </button>
            <button
              type="button"
              onClick={() => { setCreatingWs(false); setWsName(""); }}
              className="px-4 py-2 text-sm text-zinc-400 hover:text-zinc-200 border border-zinc-700 rounded-md transition-colors"
            >
              Cancel
            </button>
          </div>
        </form>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      {/* workspace switcher */}
      <div role="toolbar" aria-label="Workspaces" className="flex items-center gap-2 px-6 pt-5 pb-2 flex-wrap">
        {workspaces.map(ws => (
          <button
            key={ws.id}
            onClick={() => setActiveWs(ws.id)}
            aria-pressed={activeWs === ws.id}
            className={`px-3 py-1.5 rounded-md text-sm font-medium border transition-colors ${
              activeWs === ws.id
                ? "bg-zinc-800 border-zinc-600 text-zinc-100"
                : "bg-transparent border-zinc-700 text-zinc-500 hover:text-zinc-300"
            }`}
          >
            {ws.name}
            <span className="ml-1.5 text-xs text-zinc-600">{ws.kind}</span>
          </button>
        ))}
        <button
          onClick={() => setCreatingWs(true)}
          aria-label="Create new workspace"
          className="px-3 py-1.5 rounded-md text-sm border border-dashed border-zinc-700 text-zinc-600 hover:text-zinc-400 hover:border-zinc-500 transition-colors"
        >
          + New workspace
        </button>
      </div>

      {/* tabs */}
      <div role="tablist" aria-label="Content type" className="flex gap-0 px-6 border-b border-zinc-700 mt-2">
        {(["projects", "my-tasks"] as const).map(t => (
          <button
            key={t}
            role="tab"
            aria-selected={tab === t}
            onClick={() => setTab(t)}
            className={`px-4 py-2 text-sm border-b-2 transition-colors ${
              tab === t
                ? "border-accent text-zinc-100"
                : "border-transparent text-zinc-500 hover:text-zinc-300"
            }`}
          >
            {t === "projects" ? "Projects" : "My Tasks"}
          </button>
        ))}
      </div>

      <div className="flex-1 overflow-y-auto p-6">
        {tab === "projects" && (
          <div>
            {/* inline create-project form */}
            {creatingProject && (
              <form
                onSubmit={submitCreateProject}
                className="mb-6 bg-zinc-800 border border-zinc-700 rounded-xl p-5 max-w-sm flex flex-col gap-4"
              >
                <h3 className="font-semibold text-sm text-zinc-100">New project</h3>
                <div className="flex flex-col gap-1.5">
                  <label htmlFor="proj-name" className="text-xs text-zinc-500">Name</label>
                  <input
                    id="proj-name"
                    autoFocus
                    value={projName}
                    onChange={e => setProjName(e.target.value)}
                    placeholder="My project"
                    required
                    className="bg-zinc-900 border border-zinc-600 rounded-md px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
                  />
                </div>
                <div className="flex flex-col gap-1.5">
                  <label htmlFor="proj-key" className="text-xs text-zinc-500">Key <span className="text-zinc-600">(auto-generated, editable)</span></label>
                  <input
                    id="proj-key"
                    value={projKey}
                    onChange={e => setProjKey(e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 6))}
                    placeholder="PRJ"
                    required
                    className="bg-zinc-900 border border-zinc-600 rounded-md px-3 py-2 text-sm text-zinc-100 font-mono focus:outline-none focus:border-zinc-400"
                  />
                </div>
                <div className="flex gap-2">
                  <button
                    type="submit"
                    disabled={!projName.trim() || !projKey || projLoading}
                    className="flex-1 py-2 rounded-md bg-accent text-zinc-900 font-semibold text-sm hover:opacity-90 disabled:opacity-50 transition-opacity"
                  >
                    {projLoading ? "Creating…" : "Create"}
                  </button>
                  <button
                    type="button"
                    onClick={() => { setCreatingProject(false); setProjName(""); }}
                    className="px-4 py-2 text-sm text-zinc-400 hover:text-zinc-200 border border-zinc-700 rounded-md transition-colors"
                  >
                    Cancel
                  </button>
                </div>
              </form>
            )}

            <div className="flex items-center justify-between mb-4">
              <span className="text-[10px] font-semibold uppercase tracking-widest text-zinc-500">
                Projects{projects.length > 0 ? ` · ${projects.length}` : ""}
              </span>
              {!creatingProject && (
                <button
                  onClick={() => setCreatingProject(true)}
                  className="text-xs text-zinc-500 hover:text-zinc-200 transition-colors flex items-center gap-0.5"
                >
                  + New project
                </button>
              )}
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
              {projects.length === 0 && !creatingProject && (
                <div className="col-span-full flex flex-col items-start gap-2">
                  <p className="text-sm text-zinc-500">No projects yet.</p>
                  <button
                    onClick={() => setCreatingProject(true)}
                    className="text-sm text-accent hover:opacity-80 transition-opacity"
                  >
                    + Create the first project
                  </button>
                </div>
              )}
              {projects.map(p => (
                <button
                  key={p.id}
                  onClick={() => onSelectProject(p)}
                  className="text-left bg-zinc-800/40 border border-zinc-700 rounded-lg p-4 hover:border-zinc-600 hover:bg-zinc-800 transition-colors"
                >
                  <div className="flex items-start justify-between gap-2">
                    <span className="font-semibold text-zinc-100">{p.name}</span>
                    <span className="text-xs font-mono text-zinc-500 bg-zinc-800 px-1.5 py-0.5 rounded">{p.key}</span>
                  </div>
                  {p.description && (
                    <p className="text-sm text-zinc-400 mt-1 line-clamp-2">{p.description}</p>
                  )}
                  <div className="mt-3 flex gap-3 text-xs text-zinc-600">
                    <span>{p.taskCount} task{p.taskCount !== 1 ? "s" : ""}</span>
                    {p.startDate && <span>Started {p.startDate}</span>}
                    <span className="ml-auto capitalize">{p.scheduleMode}</span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        )}

        {tab === "my-tasks" && (
          <div className="space-y-0.5">
            {myTasks.length === 0 && (
              <div className="text-sm text-zinc-500 py-4">No tasks assigned to you.</div>
            )}
            {myTasks.map(t => (
              <div key={t.id} className="flex items-center gap-3 px-3 py-2 rounded hover:bg-zinc-800/50 text-sm">
                <PriorityIcon priority={t.priority} />
                <span className="flex-1 text-zinc-200">{t.title}</span>
                <span className="text-xs text-zinc-600 font-mono">#{t.number}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
