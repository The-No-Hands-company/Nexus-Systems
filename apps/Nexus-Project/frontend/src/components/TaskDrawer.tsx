import { useEffect, useRef, useState } from "react";
import type { Status, Task } from "../types";
import { useApi } from "../runtime";
import { StatusBadge } from "./StatusBadge";
import { PriorityIcon } from "./PriorityIcon";

interface Props {
  task: Task;
  statuses: Status[];
  onClose: () => void;
  onUpdated: (t: Task) => void;
}

export function TaskDrawer({ task, statuses, onClose, onUpdated }: Props) {
  const api = useApi();
  const [title, setTitle] = useState(task.title);
  const [description, setDescription] = useState(task.description ?? "");
  const [statusId, setStatusId] = useState(task.statusId);
  const [priority, setPriority] = useState(task.priority);
  const [saving, setSaving] = useState(false);
  const drawerRef = useRef<HTMLDivElement>(null);

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
      const updated = await api.updateTask(task.id, { title, description: description || null, statusId, priority }, task.version);
      onUpdated(updated);
    } finally {
      setSaving(false);
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
          <span id="task-drawer-title" className="text-xs text-zinc-500 font-mono">Task #{task.number}</span>
          <button onClick={onClose} aria-label="Close" className="text-zinc-400 hover:text-zinc-100 text-xl leading-none">×</button>
        </div>

        <div className="flex-1 overflow-y-auto p-5 space-y-4">
          <div>
            <label htmlFor="task-title" className="block text-xs text-zinc-500 mb-1">Title</label>
            <input
              id="task-title"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
              value={title}
              onChange={e => setTitle(e.target.value)}
            />
          </div>

          <div>
            <label htmlFor="task-status" className="block text-xs text-zinc-500 mb-1">Status</label>
            <select
              id="task-status"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
              value={statusId}
              onChange={e => setStatusId(e.target.value)}
            >
              {statuses.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
            </select>
            {currentStatus && <div className="mt-1"><StatusBadge status={currentStatus} /></div>}
          </div>

          <div>
            <label htmlFor="task-priority" className="block text-xs text-zinc-500 mb-1">Priority</label>
            <select
              id="task-priority"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400"
              value={priority}
              onChange={e => setPriority(e.target.value as Task["priority"])}
            >
              {(["none", "low", "medium", "high", "urgent"] as const).map(p => (
                <option key={p} value={p}>{p.charAt(0).toUpperCase() + p.slice(1)}</option>
              ))}
            </select>
            <div className="mt-1"><PriorityIcon priority={priority} /></div>
          </div>

          <div>
            <label htmlFor="task-description" className="block text-xs text-zinc-500 mb-1">Description</label>
            <textarea
              id="task-description"
              className="w-full bg-zinc-800 border border-zinc-600 rounded px-3 py-2 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400 min-h-[100px] resize-y"
              value={description}
              onChange={e => setDescription(e.target.value)}
            />
          </div>

          {task.startDate && (
            <div className="text-xs text-zinc-500">
              Start: <span className="text-zinc-300">{task.startDate}</span>
            </div>
          )}
          {task.finishDate && (
            <div className="text-xs text-zinc-500">
              Finish: <span className="text-zinc-300">{task.finishDate}</span>
            </div>
          )}
          {task.deadline && (
            <div className="text-xs text-zinc-500">
              Deadline: <span className="text-red-400">{task.deadline}</span>
            </div>
          )}
        </div>

        <div className="px-5 py-4 border-t border-zinc-700 flex gap-3">
          <button
            onClick={save}
            disabled={saving}
            className="flex-1 bg-zinc-100 text-zinc-900 font-semibold text-sm py-2 rounded hover:bg-white disabled:opacity-50"
          >
            {saving ? "Saving…" : "Save"}
          </button>
          <button
            onClick={onClose}
            className="px-4 py-2 text-sm text-zinc-400 hover:text-zinc-100 border border-zinc-700 rounded"
          >
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
}
