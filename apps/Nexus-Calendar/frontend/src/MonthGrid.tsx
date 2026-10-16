import { useRef } from "react";
import type { CalEvent } from "./calendar-api";

const DAYS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
const DAY_FULL = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"];
const isoDate = (date: Date) => date.toISOString().slice(0, 10);

function overlapsDay(event: CalEvent, day: string): boolean {
  const start = new Date(`${day}T00:00:00.000Z`).getTime();
  const end = start + 86_400_000;
  return Date.parse(event.startTime) < end && Date.parse(event.endTime) > start;
}

export default function MonthGrid({ year, month, events, selectedDay, today, onSelectDay, onSelectEvent }: {
  year: number; month: number; events: CalEvent[]; selectedDay: string | null; today: string;
  onSelectDay(day: string): void; onSelectEvent(event: CalEvent): void;
}) {
  const gridRef = useRef<HTMLDivElement>(null);
  const firstOffset = (new Date(Date.UTC(year, month, 1)).getUTCDay() + 6) % 7;
  const daysInMonth = new Date(Date.UTC(year, month + 1, 0)).getUTCDate();

  const handleGridKeyDown = (e: React.KeyboardEvent) => {
    const grid = gridRef.current;
    if (!grid) return;
    const buttons = Array.from(grid.querySelectorAll<HTMLButtonElement>("button[data-date]"));
    const idx = buttons.indexOf(document.activeElement as HTMLButtonElement);
    if (idx === -1) return;
    let next = idx;
    if (e.key === "ArrowRight") next = idx + 1;
    else if (e.key === "ArrowLeft") next = idx - 1;
    else if (e.key === "ArrowDown") next = idx + 7;
    else if (e.key === "ArrowUp") next = idx - 7;
    else return;
    e.preventDefault();
    if (next >= 0 && next < buttons.length) buttons[next].focus();
  };

  const cells = Array.from({ length: firstOffset }, (_, index) => (
    <div key={`blank-${index}`} role="gridcell" aria-hidden="true" className="border-b border-r border-white/10" />
  ));

  for (let day = 1; day <= daysInMonth; day++) {
    const date = `${year}-${String(month + 1).padStart(2, "0")}-${String(day).padStart(2, "0")}`;
    const eventList = events.filter((event) => overlapsDay(event, date));
    const isToday = today === date;
    const isSelected = selectedDay === date;

    cells.push(
      <button
        key={date}
        type="button"
        data-date={date}
        role="gridcell"
        aria-label={`${date}${eventList.length > 0 ? `, ${eventList.length} event${eventList.length !== 1 ? "s" : ""}` : ""}`}
        aria-selected={isSelected}
        onClick={() => onSelectDay(date)}
        className={[
          "min-h-20 border-b border-r border-white/10 p-1 text-left hover:bg-white/5 transition-colors",
          isSelected ? "bg-white/10" : "",
          isToday ? "ring-1 ring-inset ring-accent" : "",
        ].join(" ")}
      >
        <span className={`text-xs ${isToday ? "text-accent font-semibold" : "text-zinc-400"}`}>{day}</span>
        <span className="sr-only">{isoDate(new Date(`${date}T00:00:00.000Z`))}</span>
        {eventList.slice(0, 3).map((event) => (
          <span
            key={event.id}
            role="button"
            tabIndex={0}
            onClick={(e) => { e.stopPropagation(); onSelectEvent(event); }}
            onKeyDown={(e) => { if (e.key === "Enter") { e.stopPropagation(); onSelectEvent(event); } }}
            className="mt-1 block truncate rounded bg-accent/15 px-1 text-xs text-accent"
          >
            {event.title}
          </span>
        ))}
        {eventList.length > 3 && (
          <span className="block text-xs text-zinc-500">+{eventList.length - 3} more</span>
        )}
      </button>,
    );
  }

  return (
    <div role="grid" aria-label="Calendar days">
      <div role="row" className="grid grid-cols-7 border-b border-white/10">
        {DAYS.map((day, i) => (
          <div key={day} role="columnheader" aria-label={DAY_FULL[i]} className="py-2 text-center text-xs uppercase tracking-wider text-zinc-500">{day}</div>
        ))}
      </div>
      <div
        ref={gridRef}
        onKeyDown={handleGridKeyDown}
        className="grid flex-1 grid-cols-7 auto-rows-fr overflow-y-auto"
      >
        {cells}
      </div>
    </div>
  );
}
