import { useEffect, useRef, useState } from "react";
import type { Project, Status, Task } from "../types";
import { useApi } from "../runtime";
import { PriorityIcon } from "../components/PriorityIcon";
import { TaskDrawer } from "../components/TaskDrawer";
import { AssigneeBadge } from "../components/AssigneeBadge";

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
        className="mt-1 w-full flex items-center gap-1 px-2 py-1.5 rounded text-xs text-zinc-600 hover:text-zinc-300 hover:bg-zinc-800 transition-colors"
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
  }, [project.id, api]);

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
      {statuses.map(status => {
        const columnTasks = tasksByStatus(status.id);
        return (
          <div
            key={status.id}
            className="flex-shrink-0 w-72 flex flex-col bg-zinc-900 rounded-lg border border-zinc-800"
            onDragOver={e => e.preventDefault()}
            onDrop={() => handleDrop(status.id)}
            data-testid={`column-${status.id}`}
          >
            <div className="px-3 py-2.5 border-b border-zinc-800 flex items-center justify-between">
              <span className="text-xs font-semibold text-zinc-300 uppercase tracking-wider">{status.name}</span>
              <span className="text-xs text-zinc-500 font-mono px-1.5 py-0.5 bg-zinc-800 rounded">{columnTasks.length}</span>
            </div>

            <div className="flex-1 overflow-y-auto p-2 space-y-2 min-h-[40px]">
              {columnTasks.map(task => {
                const subtasks = tasks.filter(t => t.parentId === task.id);
                const completedSubtasks = subtasks.filter(
                  t => statuses.find(s => s.id === t.statusId)?.category === "completed"
                ).length;
                const effectiveProgress = subtasks.length > 0
                  ? Math.round((completedSubtasks / subtasks.length) * 100)
                  : task.progress;

                return (
                  <button
                    key={task.id}
                    type="button"
                    aria-label={`${task.title}, task #${task.number}`}
                    draggable
                    onDragStart={() => handleDragStart(task.id)}
                    onClick={() => setSelected(task)}
                    className="w-full text-left bg-zinc-800 border border-zinc-700/80 rounded-md p-2.5 cursor-pointer hover:border-zinc-500 hover:bg-zinc-800/90 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent transition-all select-none shadow-sm"
                    data-testid={`task-card-${task.id}`}
                  >
                    <div className="flex items-start gap-1.5">
                      <PriorityIcon priority={task.priority} />
                      <span className="flex-1 text-sm text-zinc-100 font-medium leading-snug">
                        {task.kind === "milestone" && (
                          <span className="text-purple-400 mr-1" title="Milestone">◆</span>
                        )}
                        {task.title}
                      </span>
                    </div>

                    {/* Subtask checklist progress */}
                    {subtasks.length > 0 && (
                      <div className="mt-2 space-y-1">
                        <div className="flex items-center justify-between text-[11px] text-zinc-400">
                          <span className="flex items-center gap-1">
                            <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                              <polyline points="9 11 12 14 22 4" />
                              <path d="M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11" />
                            </svg>
                            {completedSubtasks}/{subtasks.length} subtasks
                          </span>
                          <span className="font-mono text-zinc-500">{effectiveProgress}%</span>
                        </div>
                        <div className="w-full bg-zinc-700/60 rounded-full h-1 overflow-hidden">
                          <div
                            className="bg-emerald-500 h-full rounded-full transition-all"
                            style={{ width: `${effectiveProgress}%` }}
                          />
                        </div>
                      </div>
                    )}

                    {/* Direct task progress (if no subtasks but task has progress) */}
                    {subtasks.length === 0 && effectiveProgress > 0 && (
                      <div className="mt-2 w-full bg-zinc-700/60 rounded-full h-1 overflow-hidden">
                        <div
                          className="bg-emerald-500 h-full rounded-full transition-all"
                          style={{ width: `${effectiveProgress}%` }}
                        />
                      </div>
                    )}

                    <div className="mt-2 pt-1.5 border-t border-zinc-700/40 flex items-center justify-between text-xs text-zinc-500">
                      <span className="font-mono text-[11px]">#{task.number}</span>
                      <div className="flex items-center gap-1.5">
                        {task.deadline && (
                          <span className="text-[10px] text-red-400/90 font-medium bg-red-950/40 px-1 rounded border border-red-900/50">
                            {task.deadline.slice(5)}
                          </span>
                        )}
                        {task.assigneeSubject && (
                          <AssigneeBadge subject={task.assigneeSubject} size="sm" />
                        )}
                      </div>
                    </div>
                  </button>
                );
              })}
              <AddTaskRow
                projectId={project.id}
                statusId={status.id}
                onCreated={(t) => setTasks(ts => [...ts, t])}
              />
            </div>
          </div>
        );
      })}

      {selected && (
        <TaskDrawer
          task={selected}
          statuses={statuses}
          allTasks={tasks}
          onTasksChanged={setTasks}
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
