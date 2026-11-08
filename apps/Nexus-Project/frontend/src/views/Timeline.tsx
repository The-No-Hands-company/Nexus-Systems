import { useEffect, useState } from "react";
import type { Dependency, Project, ScheduleResult, Status, Task } from "../types";
import { useApi } from "../runtime";
import { TaskDrawer } from "../components/TaskDrawer";

const DAY_PX = 24;
const ROW_H = 34;
const LABEL_W = 240;

interface Props { project: Project; }

function daysBetween(a: string, b: string): number {
  return Math.round((new Date(b).getTime() - new Date(a).getTime()) / 86400000);
}

function addDays(date: string, d: number): string {
  const dt = new Date(date);
  dt.setDate(dt.getDate() + d);
  return dt.toISOString().slice(0, 10);
}

export function TimelineView({ project }: Props) {
  const api = useApi();
  const [tasks, setTasks] = useState<Task[]>([]);
  const [statuses, setStatuses] = useState<Status[]>([]);
  const [dependencies, setDependencies] = useState<Dependency[]>([]);
  const [schedule, setSchedule] = useState<ScheduleResult | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [filter, setFilter] = useState<"all" | "milestones" | "critical">("all");
  const [selected, setSelected] = useState<Task | null>(null);
  const [newMilestoneActive, setNewMilestoneActive] = useState(false);
  const [newMilestoneTitle, setNewMilestoneTitle] = useState("");

  const reloadData = () => {
    Promise.all([
      api.listTasks(project.id),
      api.getSchedule(project.id),
      api.listStatuses(project.id).catch(() => []),
      api.listDependencies ? api.listDependencies(project.id).catch(() => []) : Promise.resolve([]),
    ])
      .then(([ts, sc, ss, deps]) => {
        setTasks(ts);
        setSchedule(sc);
        setStatuses(ss);
        setDependencies(deps);
      })
      .catch(e => setError(String(e)));
  };

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    Promise.all([
      api.listTasks(project.id),
      api.getSchedule(project.id),
      api.listStatuses(project.id).catch(() => []),
      api.listDependencies ? api.listDependencies(project.id).catch(() => []) : Promise.resolve([]),
    ])
      .then(([ts, sc, ss, deps]) => {
        if (cancelled) return;
        setTasks(ts);
        setSchedule(sc);
        setStatuses(ss);
        setDependencies(deps);
        setLoading(false);
      })
      .catch(e => {
        if (!cancelled) {
          setError(String(e));
          setLoading(false);
        }
      });
    return () => {
      cancelled = true;
    };
  }, [project.id, api]);

  const handleAddMilestone = async () => {
    const title = newMilestoneTitle.trim();
    if (!title) {
      setNewMilestoneActive(false);
      return;
    }
    try {
      const defaultStatusId = statuses[0]?.id;
      await api.createTask(project.id, {
        title,
        kind: "milestone",
        statusId: defaultStatusId,
        durationDays: 0,
        startDate: new Date().toISOString().slice(0, 10),
      });
      setNewMilestoneTitle("");
      setNewMilestoneActive(false);
      reloadData();
    } catch (e) {
      console.error("Failed to create milestone", e);
    }
  };

  if (loading) return <div className="p-8 text-zinc-400">Computing schedule…</div>;
  if (error) return <div className="p-8 text-red-400">{error}</div>;
  if (!schedule || tasks.length === 0) return <div className="p-8 text-zinc-500">No tasks to schedule.</div>;

  const taskScheduleById = new Map(schedule.tasks.map(s => [s.id, s]));
  const criticalSet = new Set(schedule.criticalPath);

  // Filter tasks based on view mode
  const filteredTasks = tasks.filter(task => {
    if (filter === "milestones") return task.kind === "milestone";
    if (filter === "critical") return criticalSet.has(task.id);
    return true;
  });

  const scheduledDates = schedule.tasks.filter(s => s.earlyStart && s.earlyFinish);
  const projectStart = scheduledDates.length > 0
    ? scheduledDates.reduce((min, s) => (s.earlyStart < min ? s.earlyStart : min), scheduledDates[0].earlyStart)
    : new Date().toISOString().slice(0, 10);
  const projectEnd = schedule.projectFinish ?? (
    scheduledDates.length > 0
      ? scheduledDates.reduce((max, s) => (s.earlyFinish > max ? s.earlyFinish : max), scheduledDates[0].earlyFinish)
      : addDays(projectStart, 14)
  );
  const totalDays = Math.max(daysBetween(projectStart, projectEnd) + 1, 14);

  const today = new Date().toISOString().slice(0, 10);
  const todayX = Math.max(0, daysBetween(projectStart, today));

  // Generate week labels
  const weeks: { label: string; x: number }[] = [];
  for (let d = 0; d < totalDays; d += 7) {
    weeks.push({ label: addDays(projectStart, d), x: d * DAY_PX });
  }

  const svgWidth = Math.max(totalDays * DAY_PX + LABEL_W, 800);
  const svgHeight = filteredTasks.length * ROW_H + 45;

  // Build row positions map for dependency curves
  const taskRowIndex = new Map(filteredTasks.map((t, i) => [t.id, i]));

  return (
    <div className="h-full flex flex-col overflow-hidden bg-zinc-950" data-testid="timeline-view">
      {/* Top Filter and Actions Toolbar */}
      <div className="flex flex-wrap items-center justify-between gap-3 px-4 py-2.5 border-b border-zinc-800 bg-zinc-900/60 shrink-0">
        <div className="flex items-center gap-2">
          <span className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">View:</span>
          <div className="flex rounded-md bg-zinc-800 p-0.5 border border-zinc-700">
            <button
              type="button"
              onClick={() => setFilter("all")}
              className={`px-2.5 py-1 text-xs rounded font-medium transition-colors ${
                filter === "all" ? "bg-zinc-700 text-zinc-100 shadow" : "text-zinc-400 hover:text-zinc-200"
              }`}
            >
              All Tasks ({tasks.length})
            </button>
            <button
              type="button"
              onClick={() => setFilter("milestones")}
              className={`flex items-center gap-1 px-2.5 py-1 text-xs rounded font-medium transition-colors ${
                filter === "milestones" ? "bg-purple-900/70 text-purple-200 border border-purple-700" : "text-zinc-400 hover:text-purple-300"
              }`}
            >
              <span className="text-purple-400">◆</span> Milestones ({tasks.filter(t => t.kind === "milestone").length})
            </button>
            <button
              type="button"
              onClick={() => setFilter("critical")}
              className={`flex items-center gap-1 px-2.5 py-1 text-xs rounded font-medium transition-colors ${
                filter === "critical" ? "bg-red-900/70 text-red-200 border border-red-700" : "text-zinc-400 hover:text-red-300"
              }`}
            >
              <span className="text-red-400">●</span> Critical Path ({schedule.criticalPath.length})
            </button>
          </div>
        </div>

        <div className="flex items-center gap-2">
          {newMilestoneActive ? (
            <div className="flex items-center gap-1.5">
              <input
                autoFocus
                value={newMilestoneTitle}
                onChange={e => setNewMilestoneTitle(e.target.value)}
                onKeyDown={e => {
                  if (e.key === "Enter") void handleAddMilestone();
                  if (e.key === "Escape") setNewMilestoneActive(false);
                }}
                placeholder="Milestone title…"
                className="bg-zinc-800 border border-zinc-600 rounded px-2.5 py-1 text-xs text-zinc-100 focus:outline-none focus:border-purple-500 placeholder:text-zinc-500"
              />
              <button
                type="button"
                onClick={() => void handleAddMilestone()}
                className="px-2 py-1 bg-purple-600 hover:bg-purple-500 text-white text-xs font-semibold rounded"
              >
                Create
              </button>
              <button
                type="button"
                onClick={() => setNewMilestoneActive(false)}
                className="px-2 py-1 text-zinc-400 hover:text-zinc-200 text-xs"
              >
                Cancel
              </button>
            </div>
          ) : (
            <button
              type="button"
              onClick={() => setNewMilestoneActive(true)}
              className="flex items-center gap-1 px-2.5 py-1 bg-purple-900/40 hover:bg-purple-900/70 text-purple-300 border border-purple-700/60 rounded text-xs font-medium transition-colors"
            >
              <span>+</span> Add Milestone
            </button>
          )}
        </div>
      </div>

      <div className="flex-1 overflow-auto p-4">
        {schedule.violations.length > 0 && (
          <div role="alert" className="mb-3 p-2.5 bg-red-950/60 border border-red-800 rounded text-xs text-red-300 flex items-center gap-2">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" className="shrink-0 text-red-400">
              <circle cx="12" cy="12" r="10" />
              <line x1="12" y1="8" x2="12" y2="12" />
              <line x1="12" y1="16" x2="12.01" y2="16" />
            </svg>
            <span>{schedule.violations.length} constraint violation{schedule.violations.length > 1 ? "s" : ""} detected in project dependencies</span>
          </div>
        )}

        {/* Screen-reader task list for accessibility */}
        <ul className="sr-only" aria-label="Task schedule">
          {filteredTasks.map(task => {
            const sched = taskScheduleById.get(task.id);
            return (
              <li key={task.id}>
                {task.kind === "milestone" ? "Milestone: " : ""}{task.title}: {sched ? `${sched.earlyStart} – ${sched.earlyFinish}` : "not scheduled"}
                {criticalSet.has(task.id) ? " (critical path)" : ""}
              </li>
            );
          })}
        </ul>

        <svg width={svgWidth} height={svgHeight} className="font-mono text-xs select-none" aria-hidden="true">
          <defs>
            {/* Arrowhead marker for dependency lines */}
            <marker id="dep-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
              <path d="M 0 1 L 10 5 L 0 9 z" fill="#818cf8" />
            </marker>
          </defs>

          {/* Week gridlines */}
          {weeks.map(w => (
            <g key={w.label}>
              <line x1={LABEL_W + w.x} y1={0} x2={LABEL_W + w.x} y2={svgHeight} stroke="#27272a" strokeWidth={1} />
              <text x={LABEL_W + w.x + 3} y={14} fill="#71717a" fontSize={10} fontWeight="bold">
                {w.label.slice(5)}
              </text>
            </g>
          ))}

          {/* Today vertical marker line */}
          {todayX >= 0 && todayX <= totalDays && (
            <g>
              <line
                x1={LABEL_W + todayX * DAY_PX}
                y1={0}
                x2={LABEL_W + todayX * DAY_PX}
                y2={svgHeight}
                stroke="#10b981"
                strokeWidth={1.5}
                strokeDasharray="4 2"
              />
              <rect x={LABEL_W + todayX * DAY_PX - 20} y={2} width={40} height={14} rx={2} fill="#065f46" />
              <text x={LABEL_W + todayX * DAY_PX} y={12} fill="#6ee7b7" fontSize={9} textAnchor="middle" fontWeight="bold">
                TODAY
              </text>
            </g>
          )}

          {/* Task rows */}
          {filteredTasks.map((task, i) => {
            const sched = taskScheduleById.get(task.id);
            const y = 32 + i * ROW_H;
            const isCritical = criticalSet.has(task.id);
            const isMilestone = task.kind === "milestone";

            let barX = 0, barW = 0;
            if (sched) {
              barX = LABEL_W + daysBetween(projectStart, sched.earlyStart) * DAY_PX;
              barW = Math.max(DAY_PX, daysBetween(sched.earlyStart, sched.earlyFinish) * DAY_PX);
            }

            return (
              <g
                key={task.id}
                className="cursor-pointer group"
                onClick={() => setSelected(task)}
              >
                {/* Row background */}
                <rect
                  x={0}
                  y={y}
                  width={svgWidth}
                  height={ROW_H - 2}
                  fill={i % 2 === 0 ? "#0f0f12" : "#141418"}
                  className="hover:fill-zinc-800 transition-colors"
                />

                {/* Left Label */}
                <text
                  x={8}
                  y={y + ROW_H / 2 + 4}
                  fill={isMilestone ? "#c084fc" : isCritical ? "#f87171" : "#e4e4e7"}
                  fontSize={11}
                  fontWeight={isMilestone || isCritical ? "bold" : "normal"}
                  clipPath={`url(#clip-${i})`}
                >
                  {isMilestone ? "◆ " : ""}
                  {task.title}
                </text>
                <defs>
                  <clipPath id={`clip-${i}`}>
                    <rect x={0} y={y} width={LABEL_W - 8} height={ROW_H} />
                  </clipPath>
                </defs>

                {/* Milestone diamond marker */}
                {sched && isMilestone && (
                  <g>
                    <polygon
                      points={`${barX},${y + 8} ${barX + 8},${y + ROW_H / 2 - 1} ${barX},${y + ROW_H - 10} ${barX - 8},${y + ROW_H / 2 - 1}`}
                      fill="#9333ea"
                      stroke="#c084fc"
                      strokeWidth={2}
                    />
                    <text x={barX + 12} y={y + ROW_H / 2 + 3} fill="#c084fc" fontSize={10} fontWeight="bold">
                      {sched.earlyStart.slice(5)}
                    </text>
                  </g>
                )}

                {/* Standard Task Bar */}
                {sched && !isMilestone && (
                  <>
                    <rect
                      x={barX}
                      y={y + 6}
                      width={barW}
                      height={ROW_H - 14}
                      rx={3}
                      fill={isCritical ? "#991b1b" : "#27272a"}
                      stroke={isCritical ? "#ef4444" : "#52525b"}
                      strokeWidth={1}
                    />

                    {/* Progress fill */}
                    {task.progress > 0 && (
                      <rect
                        x={barX}
                        y={y + 6}
                        width={barW * (task.progress / 100)}
                        height={ROW_H - 14}
                        rx={3}
                        fill={isCritical ? "#dc2626" : "#059669"}
                      />
                    )}

                    {/* Duration / percentage label */}
                    <text
                      x={barX + barW + 6}
                      y={y + ROW_H / 2 + 3}
                      fill="#71717a"
                      fontSize={10}
                    >
                      {task.progress > 0 ? `${task.progress}%` : sched.earlyStart.slice(5)}
                    </text>
                  </>
                )}
              </g>
            );
          })}

          {/* Dependency connection arrows */}
          {dependencies.map(dep => {
            const predIndex = taskRowIndex.get(dep.predecessorId);
            const succIndex = taskRowIndex.get(dep.successorId);
            if (predIndex === undefined || succIndex === undefined) return null;

            const predSched = taskScheduleById.get(dep.predecessorId);
            const succSched = taskScheduleById.get(dep.successorId);
            if (!predSched || !succSched) return null;

            const startX = LABEL_W + (daysBetween(projectStart, predSched.earlyFinish) + 1) * DAY_PX;
            const startY = 32 + predIndex * ROW_H + ROW_H / 2;
            const endX = LABEL_W + daysBetween(projectStart, succSched.earlyStart) * DAY_PX;
            const endY = 32 + succIndex * ROW_H + ROW_H / 2;

            const midX = startX + 10;
            const pathData = `M ${startX} ${startY} L ${midX} ${startY} L ${midX} ${endY} L ${endX} ${endY}`;

            return (
              <path
                key={dep.id}
                d={pathData}
                fill="none"
                stroke="#6366f1"
                strokeWidth={1.5}
                strokeDasharray="3 2"
                markerEnd="url(#dep-arrow)"
                opacity={0.8}
              />
            );
          })}
        </svg>
      </div>

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
            reloadData();
          }}
        />
      )}
    </div>
  );
}
