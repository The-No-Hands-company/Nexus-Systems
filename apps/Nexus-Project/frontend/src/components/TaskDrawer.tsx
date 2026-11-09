import { useEffect, useRef, useState } from "react";
import type { Status, Task, Priority, TaskKind } from "../types";
import { useApi } from "../runtime";
import { StatusBadge } from "./StatusBadge";
import { PriorityIcon } from "./PriorityIcon";
import { AssigneeBadge } from "./AssigneeBadge";
import { SubtaskChecklist } from "./SubtaskChecklist";
import { TaskActivityLog } from "./TaskActivityLog";

interface Props {
  task: Task;
  statuses: Status[];
  onClose: () => void;
  onUpdated: (t: Task) => void;
  allTasks?: Task[];
  onTasksChanged?: (tasks: Task[]) => void;
}

interface UserCandidate {
  subject: string;
  name: string;
  email?: string;
}

export function TaskDrawer({
  task,
  statuses,
  onClose,
  onUpdated,
  allTasks,
  onTasksChanged,
}: Props) {
  const api = useApi();
  const [title, setTitle] = useState(task.title);
  const [description, setDescription] = useState(task.description ?? "");
  const [statusId, setStatusId] = useState(task.statusId);
  const [priority, setPriority] = useState<Priority>(task.priority);
  const [kind, setKind] = useState<TaskKind>(task.kind);
  const [progress, setProgress] = useState(task.progress);
  const [assigneeSubject, setAssigneeSubject] = useState<string | null>(task.assigneeSubject);
  const [saving, setSaving] = useState(false);
  const [subtasks, setSubtasks] = useState<Task[]>([]);
  const [assigneeOptions, setAssigneeOptions] = useState<UserCandidate[]>([]);
  const drawerRef = useRef<HTMLDivElement>(null);

  // Sync subtasks from props or fetch
  useEffect(() => {
    if (allTasks) {
      setSubtasks(allTasks.filter(t => t.parentId === task.id));
    } else {
      api.listTasks(task.projectId, { parent: task.id })
        .then(ts => setSubtasks(ts))
        .catch(() => {});
    }
  }, [task.id, task.projectId, allTasks, api]);

  // Load assignees from Auth and Project members
  useEffect(() => {
    let cancelled = false;
    Promise.all([
      api.listProjectMembers(task.projectId),
      api.listAuthUsers(),
    ])
      .then(([members, authUsers]) => {
        if (cancelled) return;
        const candidates = new Map<string, UserCandidate>();

        // Add from project members
        for (const m of members) {
          candidates.set(m.subject, {
            subject: m.subject,
            name: m.subject.replace("did:key:", "did:…").replace(/^user_/, ""),
          });
        }

        // Add from auth directory
        for (const u of authUsers) {
          const subject = u.id || u.username;
          candidates.set(subject, {
            subject,
            name: u.username,
            email: u.email,
          });
        }

        // Ensure current assignee is in candidates if not present
        if (task.assigneeSubject && !candidates.has(task.assigneeSubject)) {
          candidates.set(task.assigneeSubject, {
            subject: task.assigneeSubject,
            name: task.assigneeSubject,
          });
        }

        setAssigneeOptions(Array.from(candidates.values()));
      })
      .catch(() => {});

    return () => {
      cancelled = true;
    };
  }, [task.projectId, task.assigneeSubject, api]);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (drawerRef.current && !drawerRef.current.contains(e.target as Node)) onClose();
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [onClose]);

  const save = async () => {
    setSaving(true);
    try {
      const updated = await api.updateTask(
        task.id,
        {
          title,
          description: description || null,
          statusId,
          priority,
          kind,
          progress,
          assigneeSubject,
        },
        task.version
      );
      onUpdated(updated);
    } finally {
      setSaving(false);
    }
  };

  const handleSubtasksChanged = (newSubtasks: Task[]) => {
    setSubtasks(newSubtasks);
    if (allTasks && onTasksChanged) {
      const subtaskIds = new Set(newSubtasks.map(s => s.id));
      const filtered = allTasks.filter(t => t.parentId !== task.id && !subtaskIds.has(t.id));
      onTasksChanged([...filtered, ...newSubtasks]);
    }
  };

  const currentStatus = statuses.find(s => s.id === statusId);

  return (
    <div className="fixed inset-0 z-50 flex justify-end bg-black/40">
      <div
        ref={drawerRef}
        role="dialog"
        aria-modal="true"
        aria-labelledby="task-drawer-title"
        className="w-full max-w-lg bg-zinc-900 border-l border-zinc-700 flex flex-col h-full shadow-2xl"
        data-testid="task-drawer"
      >
        <div className="flex items-center justify-between px-5 py-4 border-b border-zinc-700">
          <div className="flex items-center gap-2">
            <span id="task-drawer-title" className="text-xs text-zinc-500 font-mono">
              Task #{task.number}
            </span>
            {kind === "milestone" && (
              <span className="px-2 py-0.5 rounded text-[10px] font-bold uppercase bg-purple-900/60 text-purple-300 border border-purple-700">
                ◆ Milestone
              </span>
            )}
          </div>
          <button
            onClick={onClose}
            type="button"
            aria-label="Close task"
            className="flex items-center justify-center min-h-[44px] min-w-[44px] -m-2 rounded text-zinc-400 hover:text-zinc-100 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent text-xl leading-none transition-colors"
          >
            ×
          </button>
        </div>

        <div className="flex-1 overflow-y-auto p-5 space-y-4">
          <div>
            <label htmlFor="task-title" className="block text-xs text-zinc-500 mb-1">
              Title
            </label>
            <input
              id="task-title"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
              value={title}
              onChange={e => setTitle(e.target.value)}
            />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label htmlFor="task-status" className="block text-xs text-zinc-500 mb-1">
                Status
              </label>
              <select
                id="task-status"
                className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
                value={statusId}
                onChange={e => setStatusId(e.target.value)}
              >
                {statuses.map(s => (
                  <option key={s.id} value={s.id}>
                    {s.name}
                  </option>
                ))}
              </select>
              {currentStatus && (
                <div className="mt-1">
                  <StatusBadge status={currentStatus} />
                </div>
              )}
            </div>

            <div>
              <label htmlFor="task-priority" className="block text-xs text-zinc-500 mb-1">
                Priority
              </label>
              <select
                id="task-priority"
                className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
                value={priority}
                onChange={e => setPriority(e.target.value as Priority)}
              >
                {(["none", "low", "medium", "high", "urgent"] as const).map(p => (
                  <option key={p} value={p}>
                    {p.charAt(0).toUpperCase() + p.slice(1)}
                  </option>
                ))}
              </select>
              <div className="mt-1">
                <PriorityIcon priority={priority} />
              </div>
            </div>
          </div>

          {/* Assignee Picker */}
          <div>
            <label htmlFor="task-assignee" className="block text-xs text-zinc-500 mb-1">
              Assignee
            </label>
            <div className="flex items-center gap-2">
              <select
                id="task-assignee"
                className="flex-1 bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
                value={assigneeSubject ?? ""}
                onChange={e => setAssigneeSubject(e.target.value || null)}
              >
                <option value="">Unassigned</option>
                {assigneeOptions.map(opt => (
                  <option key={opt.subject} value={opt.subject}>
                    {opt.name} {opt.email ? `(${opt.email})` : ""}
                  </option>
                ))}
              </select>
              {assigneeSubject && <AssigneeBadge subject={assigneeSubject} size="md" />}
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label htmlFor="task-kind" className="block text-xs text-zinc-500 mb-1">
                Type
              </label>
              <select
                id="task-kind"
                className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
                value={kind}
                onChange={e => setKind(e.target.value as TaskKind)}
              >
                <option value="task">Standard Task</option>
                <option value="milestone">Milestone</option>
              </select>
            </div>

            <div>
              <label htmlFor="task-progress" className="block text-xs text-zinc-500 mb-1">
                Progress ({progress}%)
              </label>
              <input
                id="task-progress"
                type="range"
                min="0"
                max="100"
                step="5"
                value={progress}
                onChange={e => setProgress(Number(e.target.value))}
                className="w-full h-2 bg-zinc-700 rounded-lg appearance-none cursor-pointer accent-emerald-500 mt-3"
              />
            </div>
          </div>

          <div>
            <label htmlFor="task-description" className="block text-xs text-zinc-500 mb-1">
              Description
            </label>
            <textarea
              id="task-description"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400 min-h-[90px] resize-y"
              value={description}
              onChange={e => setDescription(e.target.value)}
            />
          </div>

          {/* Subtask Checklist */}
          {kind === "task" && (
            <SubtaskChecklist
              parentTask={task}
              subtasks={subtasks}
              statuses={statuses}
              onSubtasksChanged={handleSubtasksChanged}
            />
          )}

          <div className="grid grid-cols-3 gap-2 pt-2 text-xs border-t border-zinc-800">
            <div>
              <span className="text-zinc-500">Start:</span>{" "}
              <span className="text-zinc-300">{task.startDate ?? "—"}</span>
            </div>
            <div>
              <span className="text-zinc-500">Finish:</span>{" "}
              <span className="text-zinc-300">{task.finishDate ?? "—"}</span>
            </div>
            <div>
              <span className="text-zinc-500">Deadline:</span>{" "}
              <span className={task.deadline ? "text-red-400 font-semibold" : "text-zinc-500"}>
                {task.deadline ?? "—"}
              </span>
            </div>
          </div>

          {/* Activity & Comment Log */}
          <TaskActivityLog task={task} />
        </div>

        <div className="px-5 py-4 border-t border-zinc-700 flex gap-3">
          <button
            onClick={save}
            disabled={saving}
            className="flex-1 bg-zinc-100 text-zinc-900 font-semibold text-sm py-2 rounded hover:bg-white disabled:opacity-50 transition-colors"
          >
            {saving ? "Saving…" : "Save Changes"}
          </button>
          <button
            onClick={onClose}
            className="px-4 py-2 text-sm text-zinc-400 hover:text-zinc-100 border border-zinc-700 rounded transition-colors"
          >
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
}
