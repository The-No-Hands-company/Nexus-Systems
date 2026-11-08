import { useState } from "react";
import type { Status, Task } from "../types";
import { useApi } from "../runtime";
import { AssigneeBadge } from "./AssigneeBadge";

interface Props {
  parentTask: Task;
  subtasks: Task[];
  statuses: Status[];
  onSubtasksChanged: (subtasks: Task[]) => void;
}

export function SubtaskChecklist({
  parentTask,
  subtasks,
  statuses,
  onSubtasksChanged,
}: Props) {
  const api = useApi();
  const [newTitle, setNewTitle] = useState("");
  const [adding, setAdding] = useState(false);

  const completedStatus = statuses.find(s => s.category === "completed");
  const defaultStatus = statuses.find(s => s.category === "unstarted") ?? statuses[0];

  const completedCount = subtasks.filter(
    t => statuses.find(s => s.id === t.statusId)?.category === "completed"
  ).length;
  const totalCount = subtasks.length;
  const progressPercent = totalCount > 0 ? Math.round((completedCount / totalCount) * 100) : 0;

  const toggleComplete = async (subtask: Task) => {
    const isCompleted = statuses.find(s => s.id === subtask.statusId)?.category === "completed";
    const targetStatusId = isCompleted ? (defaultStatus?.id ?? statuses[0]?.id) : (completedStatus?.id ?? subtask.statusId);

    // Optimistic update
    const updated = { ...subtask, statusId: targetStatusId };
    onSubtasksChanged(subtasks.map(t => (t.id === subtask.id ? updated : t)));

    try {
      const saved = await api.updateTask(subtask.id, { statusId: targetStatusId }, subtask.version);
      onSubtasksChanged(subtasks.map(t => (t.id === subtask.id ? saved : t)));
    } catch {
      // Revert on error
      onSubtasksChanged(subtasks);
    }
  };

  const handleAddSubtask = async () => {
    const title = newTitle.trim();
    if (!title || !defaultStatus) return;

    setAdding(true);
    try {
      const created = await api.createTask(parentTask.projectId, {
        title,
        parentId: parentTask.id,
        statusId: defaultStatus.id,
      });
      onSubtasksChanged([...subtasks, created]);
      setNewTitle("");
    } catch (e) {
      console.error("Failed to add subtask", e);
    } finally {
      setAdding(false);
    }
  };

  const handleDelete = async (subtaskId: string) => {
    onSubtasksChanged(subtasks.filter(t => t.id !== subtaskId));
    try {
      await api.deleteTask(subtaskId);
    } catch {
      onSubtasksChanged(subtasks);
    }
  };

  return (
    <div className="space-y-3 pt-2 border-t border-zinc-800">
      <div className="flex items-center justify-between">
        <label className="text-xs font-semibold text-zinc-300 uppercase tracking-wider">
          Subtasks {totalCount > 0 && <span className="text-zinc-500 font-normal">({completedCount}/{totalCount} · {progressPercent}%)</span>}
        </label>
      </div>

      {totalCount > 0 && (
        <div className="w-full bg-zinc-800 rounded-full h-1.5 overflow-hidden">
          <div
            className="bg-emerald-500 h-full transition-all duration-300 rounded-full"
            style={{ width: `${progressPercent}%` }}
          />
        </div>
      )}

      <div className="space-y-1.5 max-h-56 overflow-y-auto">
        {subtasks.map(sub => {
          const isDone = statuses.find(s => s.id === sub.statusId)?.category === "completed";
          return (
            <div
              key={sub.id}
              className="group flex items-center justify-between gap-2 px-2.5 py-1.5 bg-zinc-800/60 hover:bg-zinc-800 rounded border border-zinc-700/60 transition-colors"
            >
              <label className="flex items-center gap-2.5 flex-1 min-w-0 cursor-pointer">
                <input
                  type="checkbox"
                  checked={isDone}
                  onChange={() => void toggleComplete(sub)}
                  className="rounded border-zinc-600 text-emerald-500 focus:ring-0 focus:ring-offset-0 bg-zinc-900 cursor-pointer"
                />
                <span
                  className={`text-sm truncate select-none ${
                    isDone ? "line-through text-zinc-500" : "text-zinc-200"
                  }`}
                >
                  {sub.title}
                </span>
              </label>

              <div className="flex items-center gap-2 shrink-0">
                {sub.assigneeSubject && <AssigneeBadge subject={sub.assigneeSubject} size="sm" />}
                <button
                  type="button"
                  onClick={() => void handleDelete(sub.id)}
                  aria-label={`Delete subtask ${sub.title}`}
                  className="opacity-0 group-hover:opacity-100 text-zinc-500 hover:text-red-400 p-0.5 rounded transition-opacity"
                >
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                    <line x1="18" y1="6" x2="6" y2="18" />
                    <line x1="6" y1="6" x2="18" y2="18" />
                  </svg>
                </button>
              </div>
            </div>
          );
        })}
      </div>

      <div className="flex items-center gap-2 mt-2">
        <input
          value={newTitle}
          onChange={e => setNewTitle(e.target.value)}
          onKeyDown={e => {
            if (e.key === "Enter") {
              e.preventDefault();
              void handleAddSubtask();
            }
          }}
          disabled={adding}
          placeholder="+ Add subtask (press Enter)…"
          className="flex-1 bg-zinc-800 border border-zinc-700 rounded px-2.5 py-1.5 text-xs text-zinc-200 placeholder:text-zinc-500 focus:outline-none focus:border-zinc-500"
        />
        {newTitle.trim() && (
          <button
            type="button"
            onClick={() => void handleAddSubtask()}
            disabled={adding}
            className="px-2.5 py-1.5 bg-zinc-700 hover:bg-zinc-600 text-xs text-zinc-100 rounded font-medium transition-colors"
          >
            Add
          </button>
        )}
      </div>
    </div>
  );
}
