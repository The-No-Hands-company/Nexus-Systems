import { useEffect, useState } from "react";
import type { Project, Status, Task } from "../types";
import { useApi } from "../runtime";
import { PriorityIcon } from "../components/PriorityIcon";
import { StatusBadge } from "../components/StatusBadge";
import { TaskDrawer } from "../components/TaskDrawer";

interface Props { project: Project; }

function AddTaskRow({ projectId, defaultStatusId, onCreated }: {
  projectId: string; defaultStatusId: string; onCreated: (t: Task) => void;
}) {
  const api = useApi();
  const [active, setActive] = useState(false);
  const [title, setTitle] = useState("");

  const submit = async () => {
    if (!title.trim()) { setActive(false); return; }
    try {
      const task = await api.createTask(projectId, { title: title.trim(), statusId: defaultStatusId });
      onCreated(task);
      setTitle("");
    } catch { /* keep visible so user can retry */ }
    setActive(false);
  };

  if (!active) {
    return (
      <tr>
        <td colSpan={6} className="py-1">
          <button
            type="button"
            onClick={() => setActive(true)}
            className="flex items-center gap-1 px-3 py-1.5 text-xs text-zinc-700 hover:text-zinc-400 hover:bg-zinc-800 rounded transition-colors w-full"
          >
            + Add task
          </button>
        </td>
      </tr>
    );
  }

  return (
    <tr>
      <td className="py-2 px-3 text-zinc-600 font-mono text-xs">—</td>
      <td className="py-2 px-3" colSpan={5}>
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
          className="w-full bg-zinc-800 border border-zinc-600 rounded px-2 py-1 text-sm text-zinc-100 focus:outline-none focus:border-zinc-400 placeholder:text-zinc-600"
        />
      </td>
    </tr>
  );
}

interface TreeNode { task: Task; children: TreeNode[]; depth: number; }

function buildTree(tasks: Task[]): TreeNode[] {
  const byId = new Map(tasks.map(t => [t.id, { task: t, children: [] as TreeNode[], depth: 0 }]));
  const roots: TreeNode[] = [];
  for (const node of byId.values()) {
    const parent = node.task.parentId ? byId.get(node.task.parentId) : null;
    if (parent) { node.depth = parent.depth + 1; parent.children.push(node); }
    else roots.push(node);
  }
  return roots;
}

function flattenTree(nodes: TreeNode[]): TreeNode[] {
  const result: TreeNode[] = [];
  const walk = (ns: TreeNode[]) => {
    for (const n of ns.sort((a, b) => a.task.position - b.task.position)) {
      result.push(n);
      walk(n.children);
    }
  };
  walk(nodes);
  return result;
}

export function ListView({ project }: Props) {
  const api = useApi();
  const [statuses, setStatuses] = useState<Status[]>([]);
  const [tasks, setTasks] = useState<Task[]>([]);
  const [selected, setSelected] = useState<Task | null>(null);
  const [loading, setLoading] = useState(true);
  const statusById = new Map(statuses.map(s => [s.id, s]));

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    Promise.all([api.listStatuses(project.id), api.listTasks(project.id)])
      .then(([ss, ts]) => {
        if (cancelled) return;
        setStatuses(ss);
        setTasks(ts);
        setLoading(false);
      }).catch(() => setLoading(false));
    return () => { cancelled = true; };
  }, [project.id]);

  const rows = flattenTree(buildTree(tasks));

  if (loading) return <div className="p-8 text-zinc-400">Loading list…</div>;

  return (
    <div className="h-full overflow-auto p-4">
      <table className="w-full text-sm border-collapse" data-testid="list-table">
        <caption className="sr-only">Tasks in {project.name}</caption>
        <thead>
          <tr className="border-b border-zinc-700 text-xs text-zinc-500 uppercase tracking-wider">
            <th scope="col" className="text-left py-2 px-3 w-8">#</th>
            <th scope="col" className="text-left py-2 px-3">Title</th>
            <th scope="col" className="text-left py-2 px-3 w-32">Status</th>
            <th scope="col" aria-label="Priority" className="text-left py-2 px-3 w-10">P</th>
            <th scope="col" className="text-left py-2 px-3 w-24">Start</th>
            <th scope="col" className="text-left py-2 px-3 w-24">Finish</th>
          </tr>
        </thead>
        <tbody>
          {rows.map(({ task, depth }) => {
            const status = statusById.get(task.statusId);
            return (
              <tr
                key={task.id}
                tabIndex={0}
                aria-label={task.title}
                onClick={() => setSelected(task)}
                onKeyDown={e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); setSelected(task); } }}
                className="border-b border-zinc-800 hover:bg-zinc-800/50 cursor-pointer focus-visible:outline focus-visible:outline-2 focus-visible:outline-accent"
                data-testid={`list-row-${task.id}`}
              >
                <td className="py-2 px-3 text-zinc-600 font-mono text-xs">{task.number}</td>
                <td className="py-2 px-3">
                  <span style={{ paddingLeft: `${depth * 20}px` }} className="flex items-center gap-1">
                    {depth > 0 && <span className="text-zinc-600 text-xs">└</span>}
                    <span className="text-zinc-100">{task.title}</span>
                  </span>
                </td>
                <td className="py-2 px-3">
                  {status && <StatusBadge status={status} />}
                </td>
                <td className="py-2 px-3"><PriorityIcon priority={task.priority} /></td>
                <td className="py-2 px-3 text-zinc-400 text-xs">{task.startDate ?? "—"}</td>
                <td className="py-2 px-3 text-zinc-400 text-xs">{task.finishDate ?? "—"}</td>
              </tr>
            );
          })}
          {rows.length === 0 && (
            <tr><td colSpan={6} className="py-8 text-center text-zinc-500">No tasks yet.</td></tr>
          )}
          {statuses.length > 0 && (
            <AddTaskRow
              projectId={project.id}
              defaultStatusId={statuses[0].id}
              onCreated={t => setTasks(ts => [...ts, t])}
            />
          )}
        </tbody>
      </table>

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
