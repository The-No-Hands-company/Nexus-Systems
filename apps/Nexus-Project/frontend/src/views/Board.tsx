import { useEffect, useRef, useState } from "react";
import type { Project, Status, Task } from "../types";
import { useApi } from "../runtime";
import { PriorityIcon } from "../components/PriorityIcon";
import { TaskDrawer } from "../components/TaskDrawer";

interface Props { project: Project; }

function AddTaskRow({ projectId, statusId, onCreated }: {
  projectId: string; statusId: string; onCreated: (t: Task) => void;
}) {
  const api = useApi();
  const [active, setActive] = useState(false);
  const [title, setTitle] = useState("");

  const submit = async () => {
    if (!title.trim()) { setActive(false); return; }
    try {
      const task = await api.createTask(projectId, { title: title.trim(), statusId });
      onCreated(task);
      setTitle("");
    } catch { /* keep form open so user can retry */ }
    setActive(false);
  };

  if (!active) {
    return (
      <button
        type="button"
        onClick={() => setActive(true)}
        className="mt-1 w-full flex items-center gap-1 px-2 py-1.5 rounded text-xs text-zinc-700 hover:text-zinc-400 hover:bg-zinc-800 transition-colors"
      >
        + Add task
      </button>
    );
  }

  return (
    <div className="mt-1 bg-zinc-800 border border-zinc-600 rounded p-2">
      <input
        autoFocus
        value={title}
        onChange={e => setTitle(e.target.value)}
        onKeyDown={e => {
          if (e.key === "Enter") void submit();
          if (e.key === "Escape") { setActive(false); setTitle(""); }
        }}
        onBlur={submit}
        placeholder="Task title…"
        className="w-full bg-transparent text-sm text-zinc-100 outline-none placeholder:text-zinc-600"
      />
    </div>
  );
}

export function BoardView({ project }: Props) {
  const api = useApi();
  const [statuses, setStatuses] = useState<Status[]>([]);
  const [tasks, setTasks] = useState<Task[]>([]);
  const [selected, setSelected] = useState<Task | null>(null);
  const [loading, setLoading] = useState(true);
  const dragging = useRef<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    Promise.all([
      api.listStatuses(project.id),
      api.listTasks(project.id),
    ]).then(([ss, ts]) => {
      if (cancelled) return;
      setStatuses(ss);
      setTasks(ts);
      setLoading(false);
    }).catch(() => setLoading(false));
    return () => { cancelled = true; };
  }, [project.id]);

  const tasksByStatus = (statusId: string) =>
    tasks.filter(t => t.statusId === statusId && t.parentId === null)
      .sort((a, b) => a.position - b.position);

  const handleDragStart = (taskId: string) => { dragging.current = taskId; };

  const handleDrop = async (statusId: string) => {
    const taskId = dragging.current;
    if (!taskId) return;
    dragging.current = null;
    const task = tasks.find(t => t.id === taskId);
    if (!task || task.statusId === statusId) return;
    setTasks(ts => ts.map(t => t.id === taskId ? { ...t, statusId } : t));
    try {
      await api.moveTask(taskId, { statusId });
    } catch {
      setTasks(ts => ts.map(t => t.id === taskId ? { ...t, statusId: task.statusId } : t));
    }
  };

  if (loading) return <div className="p-8 text-zinc-400">Loading board…</div>;

  return (
    <div className="flex gap-3 h-full overflow-x-auto p-4 pb-6">
      {statuses.map(status => (
        <div
          key={status.id}
          className="flex-shrink-0 w-64 flex flex-col bg-zinc-900 rounded-lg border border-zinc-800"
          onDragOver={e => e.preventDefault()}
          onDrop={() => handleDrop(status.id)}
          data-testid={`column-${status.id}`}
        >
          <div className="px-3 py-2.5 border-b border-zinc-800 flex items-center justify-between">
            <span className="text-xs font-semibold text-zinc-300 uppercase tracking-wider">{status.name}</span>
            <span className="text-xs text-zinc-600">{tasksByStatus(status.id).length}</span>
          </div>

          <div className="flex-1 overflow-y-auto p-2 space-y-2 min-h-[40px]">
            {tasksByStatus(status.id).map(task => (
              <div
                key={task.id}
                draggable
                onDragStart={() => handleDragStart(task.id)}
                onClick={() => setSelected(task)}
                className="bg-zinc-800 border border-zinc-700 rounded p-2.5 cursor-pointer hover:border-zinc-500 transition-colors select-none"
                data-testid={`task-card-${task.id}`}
              >
                <div className="flex items-start gap-1.5">
                  <PriorityIcon priority={task.priority} />
                  <span className="flex-1 text-sm text-zinc-100 leading-snug">{task.title}</span>
                </div>
                <div className="mt-1.5 text-xs text-zinc-600">#{task.number}</div>
              </div>
            ))}
            <AddTaskRow
              projectId={project.id}
              statusId={status.id}
              onCreated={(t) => setTasks(ts => [...ts, t])}
            />
          </div>
        </div>
      ))}

      {selected && (
        <TaskDrawer
          task={selected}
          statuses={statuses}
          onClose={() => setSelected(null)}
          onUpdated={t => {
            setTasks(ts => ts.map(x => x.id === t.id ? t : x));
            setSelected(t);
          }}
        />
      )}
    </div>
  );
}
