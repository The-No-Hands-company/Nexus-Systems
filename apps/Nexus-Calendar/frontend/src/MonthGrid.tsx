import type { CalEvent } from "./calendar-api";

const DAYS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
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
  const firstOffset = (new Date(Date.UTC(year, month, 1)).getUTCDay() + 6) % 7;
  const daysInMonth = new Date(Date.UTC(year, month + 1, 0)).getUTCDate();
  const cells = Array.from({ length: firstOffset }, (_, index) => <div key={`blank-${index}`} aria-hidden="true" className="border-b border-r border-white/10" />);
  for (let day = 1; day <= daysInMonth; day++) {
    const date = `${year}-${String(month + 1).padStart(2, "0")}-${String(day).padStart(2, "0")}`;
    const eventList = events.filter((event) => overlapsDay(event, date));
    cells.push(
      <button key={date} type="button" aria-label={`Select ${date}`} onClick={() => onSelectDay(date)}
        className={`min-h-20 border-b border-r border-white/10 p-1 text-left hover:bg-white/5 ${selectedDay === date ? "bg-white/10" : ""} ${today === date ? "ring-1 ring-inset ring-[#ccff00]" : ""}`}>
        <span className={`text-xs ${today === date ? "text-[#ccff00]" : "text-zinc-400"}`}>{day}</span>
        <span className="sr-only">{isoDate(new Date(`${date}T00:00:00.000Z`))}</span>
        {eventList.slice(0, 3).map((event) => <span key={event.id} role="button" tabIndex={0} onClick={(click) => { click.stopPropagation(); onSelectEvent(event); }} onKeyDown={(key) => { if (key.key === "Enter") onSelectEvent(event); }} className="mt-1 block truncate rounded bg-[#ccff00]/15 px-1 text-xs text-[#dfff80]">{event.title}</span>)}
        {eventList.length > 3 && <span className="block text-xs text-zinc-500">+{eventList.length - 3} more</span>}
      </button>,
    );
  }
  return <><div className="grid grid-cols-7 border-b border-white/10">{DAYS.map((day) => <div key={day} className="py-2 text-center text-xs uppercase text-zinc-500">{day}</div>)}</div><div className="grid flex-1 grid-cols-7 auto-rows-fr overflow-y-auto">{cells}</div></>;
}
