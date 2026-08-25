import { useState, useEffect, useCallback } from "react";

interface CalEvent {
  id: string;
  title: string;
  description?: string;
  location?: string;
  startTime: string;
  endTime: string;
  allDay: boolean;
}

const MONTHS = ["January","February","March","April","May","June","July","August","September","October","November","December"];
const DAYS = ["Mon","Tue","Wed","Thu","Fri","Sat","Sun"];

function api(path: string, init?: RequestInit) {
  return fetch(`/ipa/calendar${path}`, {
    headers: { "content-type": "application/json" },
    credentials: "same-origin",
    ...init,
  }).then(r => r.json());
}

export default function CalendarView() {
  const now = new Date();
  const [year, setYear] = useState(now.getFullYear());
  const [month, setMonth] = useState(now.getMonth());
  const [events, setEvents] = useState<CalEvent[]>([]);
  const [selectedDay, setSelectedDay] = useState<string | null>(null);
  const [showForm, setShowForm] = useState(false);

  const fetchEvents = useCallback(async () => {
    const from = `${year}-${String(month + 1).padStart(2, "0")}-01`;
    const lastDay = new Date(year, month + 1, 0).getDate();
    const to = `${year}-${String(month + 1).padStart(2, "0")}-${lastDay}`;
    const data = await api(`/events?from=${from}&to=${to}T23:59:59`);
    setEvents(data.events ?? []);
  }, [year, month]);

  useEffect(() => { void fetchEvents(); }, [fetchEvents]);

  const firstDay = new Date(year, month, 1);
  const startOffset = (firstDay.getDay() + 6) % 7;
  const daysInMonth = new Date(year, month + 1, 0).getDate();
  const todayStr = now.toISOString().slice(0, 10);

  function prevMonth() { month === 0 ? (setMonth(11), setYear(y => y - 1)) : setMonth(m => m - 1); }
  function nextMonth() { month === 11 ? (setMonth(0), setYear(y => y + 1)) : setMonth(m => m + 1); }

  async function createEvent(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const fd = new FormData(e.currentTarget);
    const date = selectedDay ?? todayStr;
    await api("/events", { method: "POST", body: JSON.stringify({
      title: fd.get("title"), description: fd.get("description") || undefined,
      location: fd.get("location") || undefined,
      startTime: `${date}T${fd.get("startTime")}`, endTime: `${date}T${fd.get("endTime")}`,
    })});
    setShowForm(false);
    void fetchEvents();
  }

  async function deleteEvent(id: string) {
    await api(`/events/${id}`, { method: "DELETE" });
    void fetchEvents();
  }

  const cells = [];
  for (let i = 0; i < startOffset; i++) cells.push(<div key={`p-${i}`} />);
  for (let d = 1; d <= daysInMonth; d++) {
    const ds = `${year}-${String(month + 1).padStart(2, "0")}-${String(d).padStart(2, "0")}`;
    const evs = events.filter(e => e.startTime.slice(0, 10) === ds);
    cells.push(
      <div key={d} onClick={() => setSelectedDay(ds)}
        className={`min-h-[56px] sm:min-h-[72px] p-1 sm:p-1.5 cursor-pointer border-r border-b border-zinc-800/60 transition-colors
          ${ds === selectedDay ? "bg-zinc-800" : "hover:bg-zinc-900/70"}
          ${ds === todayStr ? "ring-1 ring-inset ring-lime-500/50" : ""}`}>
        <span className={`text-xs font-medium ${ds === todayStr ? "text-lime-400" : "text-zinc-400"}`}>{d}</span>
        <div className="sm:hidden flex justify-center gap-0.5 mt-1">
          {evs.slice(0, 4).map((_, i) => <span key={i} className="h-1 w-1 rounded-full bg-lime-400" />)}
        </div>
        {evs.slice(0, 3).map(e => (
          <div key={e.id} className="hidden sm:block mt-0.5 rounded px-1 py-0.5 text-[10px] leading-tight truncate bg-lime-500/15 text-lime-300">
            {e.allDay ? e.title : `${e.startTime.slice(11,16)} ${e.title}`}
          </div>
        ))}
      </div>
    );
  }

  const selEvents = selectedDay ? events.filter(e => e.startTime.slice(0,10) === selectedDay) : [];

  return (
    <div className="flex h-full min-h-[calc(100vh-3.5rem)]">
      <div className="flex-1 flex flex-col min-w-0">
        <header className="flex items-center gap-2 sm:gap-3 border-b border-white/10 px-2 sm:px-4 h-12 shrink-0">
          <h2 className="font-semibold tracking-tight text-sm sm:text-base">Calendar</h2>
          <div className="ml-auto flex items-center gap-1 sm:gap-2">
            <button onClick={prevMonth} className="px-2 py-1 rounded hover:bg-white/10 text-sm">←</button>
            <span className="font-medium min-w-[100px] sm:min-w-[140px] text-center text-xs sm:text-sm">{MONTHS[month]} {year}</span>
            <button onClick={nextMonth} className="px-2 py-1 rounded hover:bg-white/10 text-sm">→</button>
            <button onClick={() => setShowForm(true)}
              className="ml-1 sm:ml-3 rounded bg-lime-600 px-2 sm:px-3 py-1 text-xs sm:text-sm font-medium hover:bg-lime-500">+</button>
          </div>
        </header>
        <div className="grid grid-cols-7 border-b border-white/10 shrink-0">
          {DAYS.map(d => <div key={d} className="py-1.5 text-center text-[10px] sm:text-xs font-medium text-zinc-500 uppercase">{d}</div>)}
        </div>
        <div className="grid grid-cols-7 flex-1 overflow-y-auto auto-rows-fr">{cells}</div>
      </div>

      {selectedDay && (
        <aside className="fixed inset-0 z-40 bg-[#0a0a0a] md:relative md:w-72 md:shrink-0 md:border-l border-white/10 flex flex-col overflow-y-auto md:inset-auto">
          <div className="p-4 border-b border-white/10 flex items-center justify-between">
            <h3 className="font-semibold text-sm">{new Date(selectedDay + "T00:00").toLocaleDateString(undefined, { weekday: "long", day: "numeric", month: "short" })}</h3>
            <button onClick={() => { setSelectedDay(null); setShowForm(false); }} className="md:hidden text-zinc-500 hover:text-white">✕</button>
          </div>
          <div className="flex-1 p-4 space-y-3">
            {!selEvents.length && !showForm && <p className="text-sm text-zinc-500">No events.</p>}
            {selEvents.map(e => (
              <div key={e.id} className="rounded-lg border border-zinc-800 bg-zinc-900/40 p-3 group relative">
                <div className="font-medium text-sm pr-8">{e.title}</div>
                {!e.allDay && <div className="text-xs text-zinc-500 mt-0.5">{e.startTime.slice(11,16)} – {e.endTime.slice(11,16)}</div>}
                {e.location && <div className="text-xs text-zinc-500 mt-0.5">📍 {e.location}</div>}
                {e.description && <div className="text-xs text-zinc-600 mt-1">{e.description}</div>}
                <button onClick={() => deleteEvent(e.id)} className="absolute top-2 right-2 opacity-0 group-hover:opacity-100 text-red-400 hover:text-red-300 text-xs">Delete</button>
              </div>
            ))}
            {!showForm && (
              <button onClick={() => setShowForm(true)} className="w-full rounded border border-dashed border-zinc-700 py-2 text-sm text-zinc-500 hover:border-zinc-500 hover:text-zinc-300">+ Add event</button>
            )}
            {showForm && (
              <form onSubmit={createEvent} className="space-y-2">
                <input name="title" required placeholder="Title" autoFocus className="w-full rounded bg-zinc-800 border border-zinc-700 px-2 py-1.5 text-sm placeholder-zinc-600" />
                <div className="flex gap-2">
                  <input name="startTime" type="time" required defaultValue="09:00" className="flex-1 rounded bg-zinc-800 border border-zinc-700 px-2 py-1.5 text-sm" />
                  <input name="endTime" type="time" required defaultValue="10:00" className="flex-1 rounded bg-zinc-800 border border-zinc-700 px-2 py-1.5 text-sm" />
                </div>
                <input name="location" placeholder="Location (optional)" className="w-full rounded bg-zinc-800 border border-zinc-700 px-2 py-1.5 text-sm placeholder-zinc-600" />
                <textarea name="description" placeholder="Description (optional)" rows={2} className="w-full rounded bg-zinc-800 border border-zinc-700 px-2 py-1.5 text-sm placeholder-zinc-600 resize-none" />
                <div className="flex gap-2">
                  <button type="submit" className="flex-1 rounded bg-lime-600 py-1.5 text-sm font-medium hover:bg-lime-500">Save</button>
                  <button type="button" onClick={() => setShowForm(false)} className="rounded border border-zinc-700 px-3 text-sm hover:bg-zinc-800">Cancel</button>
                </div>
              </form>
            )}
          </div>
        </aside>
      )}
    </div>
  );
}
