import { useEffect, useState } from "react";
import type { Project, ScheduleResult, Task } from "../types";
import { useApi } from "../runtime";

const DAY_PX = 24;
const ROW_H = 32;
const LABEL_W = 220;

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
  const [schedule, setSchedule] = useState<ScheduleResult | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    Promise.all([api.listTasks(project.id), api.getSchedule(project.id)])
      .then(([ts, sc]) => {
        if (cancelled) return;
        setTasks(ts);
        setSchedule(sc);
        setLoading(false);
      }).catch(e => { if (!cancelled) { setError(String(e)); setLoading(false); } });
    return () => { cancelled = true; };
  }, [project.id]);

  if (loading) return <div className="p-8 text-zinc-400">Computing schedule…</div>;
  if (error) return <div className="p-8 text-red-400">{error}</div>;
  if (!schedule || tasks.length === 0) return <div className="p-8 text-zinc-500">No tasks to schedule.</div>;

  const taskScheduleById = new Map(schedule.tasks.map(s => [s.id, s]));
  const criticalSet = new Set(schedule.criticalPath);

  const projectStart = schedule.tasks.reduce((min, s) => s.earlyStart < min ? s.earlyStart : min, schedule.tasks[0].earlyStart);
  const projectEnd = schedule.projectFinish ?? schedule.tasks.reduce((max, s) => s.earlyFinish > max ? s.earlyFinish : max, schedule.tasks[0].earlyFinish);
  const totalDays = Math.max(daysBetween(projectStart, projectEnd) + 1, 7);

  const today = new Date().toISOString().slice(0, 10);
  const todayX = Math.max(0, daysBetween(projectStart, today));

  // Generate week labels
  const weeks: { label: string; x: number }[] = [];
  for (let d = 0; d < totalDays; d += 7) {
    weeks.push({ label: addDays(projectStart, d), x: d * DAY_PX });
  }

  const svgWidth = totalDays * DAY_PX + LABEL_W;
  const svgHeight = tasks.length * ROW_H + 40;

  return (
    <div className="h-full overflow-auto p-4" data-testid="timeline-view">
      {schedule.violations.length > 0 && (
        <div role="alert" className="mb-3 p-2 bg-red-900/40 border border-red-700 rounded text-xs text-red-300">
          {schedule.violations.length} constraint violation{schedule.violations.length > 1 ? "s" : ""}
        </div>
      )}
      {/* Screen-reader task list: the SVG is pure visuals */}
      <ul className="sr-only" aria-label="Task schedule">
        {tasks.map(task => {
          const sched = new Map(schedule.tasks.map(s => [s.id, s])).get(task.id);
          return (
            <li key={task.id}>
              {task.title}: {sched ? `${sched.earlyStart} – ${sched.earlyFinish}` : "not scheduled"}
              {criticalSet.has(task.id) ? " (critical path)" : ""}
            </li>
          );
        })}
      </ul>
      <svg width={svgWidth} height={svgHeight} className="font-mono text-xs" aria-hidden="true">
        {/* Week gridlines */}
        {weeks.map(w => (
          <g key={w.label}>
            <line x1={LABEL_W + w.x} y1={0} x2={LABEL_W + w.x} y2={svgHeight} stroke="#27272a" strokeWidth={1} />
            <text x={LABEL_W + w.x + 2} y={12} fill="#52525b" fontSize={9}>{w.label.slice(5)}</text>
          </g>
        ))}

        {/* Today marker */}
        {todayX >= 0 && todayX <= totalDays && (
          <line
            x1={LABEL_W + todayX * DAY_PX}
            y1={0}
            x2={LABEL_W + todayX * DAY_PX}
            y2={svgHeight}
            stroke="#ccff00"
            strokeWidth={1}
            strokeDasharray="4 2"
          />
        )}

        {/* Task rows */}
        {tasks.map((task, i) => {
          const sched = taskScheduleById.get(task.id);
          const y = 30 + i * ROW_H;
          const isCritical = criticalSet.has(task.id);

          let barX = 0, barW = 0;
          if (sched) {
            barX = LABEL_W + daysBetween(projectStart, sched.earlyStart) * DAY_PX;
            barW = Math.max(DAY_PX, daysBetween(sched.earlyStart, sched.earlyFinish) * DAY_PX);
          }

          return (
            <g key={task.id}>
              {/* Row background */}
              <rect x={0} y={y} width={svgWidth} height={ROW_H - 2} fill={i % 2 === 0 ? "#09090b" : "#0a0a0f"} />
              {/* Label */}
              <text x={4} y={y + ROW_H / 2 + 4} fill="#a1a1aa" fontSize={10} clipPath={`url(#clip-${i})`}>
                {task.title}
              </text>
              <defs>
                <clipPath id={`clip-${i}`}>
                  <rect x={0} y={y} width={LABEL_W - 4} height={ROW_H} />
                </clipPath>
              </defs>
              {/* Bar */}
              {sched && (
                <rect
                  x={barX}
                  y={y + 6}
                  width={barW}
                  height={ROW_H - 14}
                  rx={3}
                  fill={isCritical ? "#ef4444" : "#3f3f46"}
                  stroke={isCritical ? "#f87171" : "#52525b"}
                  strokeWidth={1}
                />
              )}
              {/* Progress fill */}
              {sched && task.progress > 0 && (
                <rect
                  x={barX}
                  y={y + 6}
                  width={barW * (task.progress / 100)}
                  height={ROW_H - 14}
                  rx={3}
                  fill={isCritical ? "#dc2626" : "#71717a"}
                />
              )}
            </g>
          );
        })}
      </svg>
    </div>
  );
}
