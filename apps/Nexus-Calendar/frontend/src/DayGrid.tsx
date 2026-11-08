import type { CalEvent } from "./calendar-api";

const HOURS = Array.from({ length: 24 }, (_, i) => i);

function formatHour(h: number) {
  return `${String(h).padStart(2, "0")}:00`;
}

export interface DayGridProps {
  day: string;
  events: CalEvent[];
  today: string;
  onSelectEvent: (event: CalEvent) => void;
  onNewEventAtHour?: (hour: number) => void;
}

export default function DayGrid({
  day,
  events,
  today,
  onSelectEvent,
  onNewEventAtHour,
}: DayGridProps) {
  const dayEvents = events.filter((e) => e.startTime.startsWith(day));
  const isToday = day === today;
  const dateObj = new Date(`${day}T12:00:00Z`);
  const formattedDay = dateObj.toLocaleDateString(undefined, {
    weekday: "long",
    month: "long",
    day: "numeric",
    year: "numeric",
    timeZone: "UTC",
  });

  return (
    <div className="flex flex-1 flex-col overflow-hidden border border-white/10 rounded-lg bg-zinc-950">
      {/* Day View Header */}
      <div className="flex items-center justify-between border-b border-white/10 bg-zinc-900/80 px-4 py-3">
        <div>
          <h2 className="text-sm sm:text-base font-bold text-zinc-100 flex items-center gap-2">
            <span>{formattedDay}</span>
            {isToday && (
              <span className="rounded bg-accent/20 px-2 py-0.5 text-[10px] uppercase font-semibold text-accent">
                Today
              </span>
            )}
          </h2>
          <p className="text-xs text-zinc-400 mt-0.5">
            {dayEvents.length} scheduled event{dayEvents.length !== 1 ? "s" : ""}
          </p>
        </div>
      </div>

      {/* Hourly Timeline */}
      <div className="flex-1 overflow-y-auto">
        <div className="divide-y divide-white/5">
          {HOURS.map((hour) => {
            const hourEvents = dayEvents.filter((e) => {
              const h = parseInt(e.startTime.slice(11, 13), 10);
              return h === hour;
            });

            return (
              <div
                key={`day-hour-${hour}`}
                onClick={() => onNewEventAtHour?.(hour)}
                className="flex min-h-16 group hover:bg-white/[0.02] transition-colors"
              >
                {/* Hour Timestamp Column */}
                <div className="w-16 shrink-0 border-r border-white/10 p-2 text-right font-mono text-xs text-zinc-500 select-none">
                  {formatHour(hour)}
                </div>

                {/* Event Area */}
                <div className="flex-1 p-1.5 space-y-1.5">
                  {hourEvents.map((ev) => (
                    <div
                      key={ev.id}
                      onClick={(e) => {
                        e.stopPropagation();
                        onSelectEvent(ev);
                      }}
                      className="rounded-lg border border-accent/40 bg-accent/15 p-2.5 text-accent hover:bg-accent/25 transition-colors cursor-pointer shadow-sm"
                    >
                      <div className="flex items-center justify-between gap-2">
                        <span className="font-semibold text-sm text-zinc-100">{ev.title}</span>
                        <span className="font-mono text-xs text-accent">
                          {ev.startTime.slice(11, 16)} – {ev.endTime.slice(11, 16)}
                        </span>
                      </div>
                      {ev.location && (
                        <p className="text-xs text-zinc-400 mt-1 flex items-center gap-1">
                          <span>📍</span>
                          <span>{ev.location}</span>
                        </p>
                      )}
                      {ev.recurrence && (
                        <span className="mt-1 inline-block rounded bg-zinc-800 px-1.5 py-0.5 text-[10px] text-zinc-300 font-medium">
                          🔁 {ev.recurrence.replace(/^RRULE:/, "")}
                        </span>
                      )}
                      {ev.description && (
                        <p className="text-xs text-zinc-300 mt-1.5 line-clamp-2 leading-relaxed">
                          {ev.description}
                        </p>
                      )}
                    </div>
                  ))}
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
