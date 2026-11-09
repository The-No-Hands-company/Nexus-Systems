import { useMemo } from "react";
import type { CalEvent } from "./calendar-api";
import { parseCategory, CATEGORIES } from "./categories";

const DAYS_SHORT = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
const HOURS = Array.from({ length: 24 }, (_, i) => i);

function formatHour(h: number) {
  return `${String(h).padStart(2, "0")}:00`;
}

function getMondayOfWeek(date: Date): Date {
  const d = new Date(Date.UTC(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate()));
  const day = d.getUTCDay();
  const diff = d.getUTCDate() - day + (day === 0 ? -6 : 1);
  d.setUTCDate(diff);
  return d;
}

export interface WeekGridProps {
  currentDate: Date;
  events: CalEvent[];
  selectedDay: string | null;
  today: string;
  onSelectDay: (day: string) => void;
  onSelectEvent: (event: CalEvent) => void;
}

export default function WeekGrid({
  currentDate,
  events,
  selectedDay,
  today,
  onSelectDay,
  onSelectEvent,
}: WeekGridProps) {
  const weekDays = useMemo(() => {
    const monday = getMondayOfWeek(currentDate);
    return Array.from({ length: 7 }, (_, i) => {
      const d = new Date(monday.getTime() + i * 86_400_000);
      const iso = d.toISOString().slice(0, 10);
      return {
        iso,
        dayNum: d.getUTCDate(),
        dayName: DAYS_SHORT[i],
        isToday: iso === today,
        isSelected: iso === selectedDay,
      };
    });
  }, [currentDate, today, selectedDay]);

  return (
    <div className="flex flex-1 flex-col overflow-hidden border border-white/10 rounded-lg bg-zinc-950">
      {/* Week Header Row */}
      <div className="grid grid-cols-[3.5rem_repeat(7,1fr)] border-b border-white/10 bg-zinc-900/80 text-xs">
        <div className="p-2 border-r border-white/10 text-center font-mono text-zinc-500">
          UTC
        </div>
        {weekDays.map((wd) => (
          <button
            key={wd.iso}
            type="button"
            onClick={() => onSelectDay(wd.iso)}
            className={`p-2 border-r border-white/10 text-center transition-colors hover:bg-white/5 ${
              wd.isSelected ? "bg-white/10" : ""
            }`}
          >
            <div className={`text-[10px] uppercase font-semibold ${wd.isToday ? "text-accent" : "text-zinc-400"}`}>
              {wd.dayName}
            </div>
            <div className={`text-sm font-bold ${wd.isToday ? "text-accent" : "text-zinc-200"}`}>
              {wd.dayNum}
            </div>
          </button>
        ))}
      </div>

      {/* Hourly Scroll Area */}
      <div className="flex-1 overflow-y-auto">
        <div className="grid grid-cols-[3.5rem_repeat(7,1fr)]">
          {HOURS.map((hour) => (
            <div key={`row-${hour}`} className="contents">
              {/* Hour Label */}
              <div className="h-14 border-b border-r border-white/5 px-1 py-1 text-[11px] font-mono text-zinc-500 text-right pr-2 select-none">
                {formatHour(hour)}
              </div>

              {/* 7 Columns for this hour */}
              {weekDays.map((wd) => {
                const dayEvents = events.filter((e) => {
                  if (!e.startTime.startsWith(wd.iso)) return false;
                  const eHour = parseInt(e.startTime.slice(11, 13), 10);
                  return eHour === hour;
                });

                return (
                  <div
                    key={`${wd.iso}-${hour}`}
                    onClick={() => onSelectDay(wd.iso)}
                    className="h-14 border-b border-r border-white/5 p-0.5 relative group hover:bg-white/[0.02] transition-colors cursor-pointer"
                  >
                    {dayEvents.map((ev) => {
                      const cat = CATEGORIES[parseCategory(ev.description)];
                      return (
                        <button
                          key={ev.id}
                          type="button"
                          onClick={(e) => {
                            e.stopPropagation();
                            onSelectEvent(ev);
                          }}
                          className={`w-full truncate rounded ${cat.chipBg} px-1.5 py-0.5 text-left text-xs ${cat.chipText} hover:opacity-85 transition-colors shadow-sm block mb-1`}
                          title={`${ev.title} (${ev.startTime.slice(11, 16)} - ${ev.endTime.slice(11, 16)})`}
                        >
                          <span className="font-semibold mr-1">{ev.startTime.slice(11, 16)}</span>
                          <span>{ev.title}</span>
                        </button>
                      );
                    })}
                  </div>
                );
              })}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
