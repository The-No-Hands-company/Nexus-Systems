import { useEffect, useState } from "react";
import type { Project, Status, Task } from "../types";
import { useApi } from "../runtime";
import { PriorityIcon } from "../components/PriorityIcon";
import { StatusBadge } from "../components/StatusBadge";
import { TaskDrawer } from "../components/TaskDrawer";

interface Props { project: Project; }

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
        <thead>
          <tr className="border-b border-zinc-700 text-xs text-zinc-500 uppercase tracking-wider">
            <th className="text-left py-2 px-3 w-8">#</th>
            <th className="text-left py-2 px-3">Title</th>
            <th className="text-left py-2 px-3 w-32">Status</th>
            <th className="text-left py-2 px-3 w-10">P</th>
            <th className="text-left py-2 px-3 w-24">Start</th>
            <th className="text-left py-2 px-3 w-24">Finish</th>
          </tr>
        </thead>
        <tbody>
          {rows.map(({ task, depth }) => {
            const status = statusById.get(task.statusId);
            return (
              <tr
                key={task.id}
                onClick={() => setSelected(task)}
                className="border-b border-zinc-800 hover:bg-zinc-800/50 cursor-pointer"
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
