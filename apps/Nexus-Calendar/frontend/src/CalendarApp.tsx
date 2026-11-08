import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { AppHeader } from "../../../../packages/nexus-app-shell/src";
import type { CalendarApi, CalEvent, EventInput } from "./calendar-api";
import type { CalendarRuntime } from "./runtime";
import EventDetails from "./EventDetails";
import EventEditor from "./EventEditor";
import MonthGrid from "./MonthGrid";
import WeekGrid from "./WeekGrid";
import DayGrid from "./DayGrid";
import SharingPanel from "./SharingPanel";
import { exportEventsToIcs, parseIcsToEvents } from "./ics";

const MONTHS = [
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
];

const datePart = (date: Date) => date.toISOString().slice(0, 10);

const rangeFor = (year: number, month: number) => ({
  from: `${year}-${String(month + 1).padStart(2, "0")}-01`,
  to: `${year}-${String(month + 1).padStart(2, "0")}-${String(new Date(Date.UTC(year, month + 1, 0)).getUTCDate()).padStart(2, "0")}`,
});

const isEmbedded = () =>
  typeof window !== "undefined" && new URLSearchParams(window.location.search).get("embed") === "1";

function dashboardHref(shellContext: boolean): string {
  if (shellContext || typeof window === "undefined") return "/";
  const labels = window.location.hostname.split(".");
  return labels.length >= 3 ? `${window.location.protocol}//app.${labels.slice(1).join(".")}` : "/";
}

const readableDay = (value: string) =>
  new Date(`${value}T12:00:00Z`).toLocaleDateString(undefined, {
    weekday: "long", month: "long", day: "numeric", timeZone: "UTC",
  });

export default function CalendarApp({
  runtime,
  api,
  today = new Date(),
}: {
  runtime: CalendarRuntime;
  api: CalendarApi;
  today?: Date;
}) {
  const [year, setYear] = useState(today.getUTCFullYear());
  const [month, setMonth] = useState(today.getUTCMonth());
  const [viewMode, setViewMode] = useState<"month" | "week" | "day">("month");
  const [events, setEvents] = useState<CalEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedDay, setSelectedDay] = useState<string | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<CalEvent | null>(null);
  const [editorOpen, setEditorOpen] = useState(false);
  const [editingEvent, setEditingEvent] = useState<CalEvent | null>(null);
  const [sharingOpen, setSharingOpen] = useState(false);
  const [importing, setImporting] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const todayText = datePart(today);
  const range = useMemo(() => rangeFor(year, month), [year, month]);

  const load = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      setEvents(await api.listEvents(range.from, range.to));
    } catch (failure) {
      setError((failure as { reason?: string }).reason ?? "unavailable");
    } finally {
      setLoading(false);
    }
  }, [api, range]);

  useEffect(() => { void load(); }, [load]);

  useEffect(() => {
    document.title = `${MONTHS[month]} ${year} — Calendar`;
  }, [month, year]);

  const changeMonth = (offset: number) => {
    const next = new Date(Date.UTC(year, month + offset, 1));
    setYear(next.getUTCFullYear());
    setMonth(next.getUTCMonth());
    setSelectedDay(null);
    setSelectedEvent(null);
    setEditorOpen(false);
    setEditingEvent(null);
  };

  const closeAside = () => {
    setSelectedDay(null);
    setEditorOpen(false);
    setEditingEvent(null);
    setSelectedEvent(null);
  };

  async function saveNew(input: EventInput) {
    await api.createEvent(input);
    setEditorOpen(false);
    await load();
  }

  async function saveEdit(input: EventInput) {
    if (!editingEvent) return;
    await api.updateEvent(editingEvent.id, input);
    setEditingEvent(null);
    setSelectedEvent(null);
    await load();
  }

  async function remove() {
    if (!selectedEvent) return;
    try {
      await api.deleteEvent(selectedEvent.id);
      setSelectedEvent(null);
      await load();
    } catch {
      setError("delete_failed");
    }
  }

  function handleExportIcs() {
    const icsContent = exportEventsToIcs(events);
    const blob = new Blob([icsContent], { type: "text/calendar;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `calendar-${year}-${String(month + 1).padStart(2, "0")}.ics`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  async function handleImportIcs(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    setImporting(true);
    try {
      const text = await file.text();
      const parsed = parseIcsToEvents(text);
      for (const ev of parsed) {
        await api.createEvent(ev);
      }
      await load();
    } catch {
      setError("validation");
    } finally {
      setImporting(false);
      e.target.value = "";
    }
  }

  const selectedEvents = selectedDay
    ? events.filter((event) =>
        event.startTime.slice(0, 10) === selectedDay ||
        (Date.parse(event.startTime) < Date.parse(`${selectedDay}T23:59:59Z`) &&
          Date.parse(event.endTime) > Date.parse(`${selectedDay}T00:00:00Z`)),
      )
    : [];

  const goToToday = () => {
    setYear(today.getUTCFullYear());
    setMonth(today.getUTCMonth());
    setSelectedDay(todayText);
    setSelectedEvent(null);
    setEditingEvent(null);
    setEditorOpen(false);
    setSharingOpen(false);
  };

  const embedded = isEmbedded();

  const navButton =
    "flex min-h-11 min-w-11 items-center justify-center rounded text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 transition-colors";

  const navSlot = (
    <>
      <button type="button" aria-label="Previous month" onClick={() => changeMonth(-1)} className={navButton}>
        ←
      </button>
      <span aria-live="polite" className="min-w-0 flex-1 truncate text-center text-sm font-medium text-zinc-200 sm:flex-none sm:min-w-36">
        {MONTHS[month]} {year}
      </span>
      <button type="button" aria-label="Next month" onClick={() => changeMonth(1)} className={navButton}>
        →
      </button>
      <button
        type="button"
        onClick={goToToday}
        className="min-h-11 rounded border border-zinc-700 px-3 text-sm text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors"
      >
        Today
      </button>

      {/* View Mode Switcher */}
      <div className="inline-flex rounded-lg border border-zinc-700 bg-zinc-800 p-0.5 ml-1">
        <button
          type="button"
          onClick={() => setViewMode("month")}
          className={`rounded px-2.5 py-1 text-xs font-medium transition-colors ${
            viewMode === "month" ? "bg-zinc-900 text-zinc-100 shadow-sm" : "text-zinc-400 hover:text-zinc-200"
          }`}
        >
          Month
        </button>
        <button
          type="button"
          onClick={() => {
            if (!selectedDay) setSelectedDay(todayText);
            setViewMode("week");
          }}
          className={`rounded px-2.5 py-1 text-xs font-medium transition-colors ${
            viewMode === "week" ? "bg-zinc-900 text-zinc-100 shadow-sm" : "text-zinc-400 hover:text-zinc-200"
          }`}
        >
          Week
        </button>
        <button
          type="button"
          onClick={() => {
            if (!selectedDay) setSelectedDay(todayText);
            setViewMode("day");
          }}
          className={`rounded px-2.5 py-1 text-xs font-medium transition-colors ${
            viewMode === "day" ? "bg-zinc-900 text-zinc-100 shadow-sm" : "text-zinc-400 hover:text-zinc-200"
          }`}
        >
          Day
        </button>
      </div>

      {/* Export & Import .ics */}
      <div className="hidden sm:inline-flex items-center gap-1 ml-auto">
        <button
          type="button"
          onClick={handleExportIcs}
          title="Export current calendar to RFC 5545 .ics"
          className="min-h-11 rounded border border-zinc-700 px-2.5 text-xs text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors"
        >
          Export .ics
        </button>
        <label
          title="Import .ics events from Google/Apple Calendar"
          className="min-h-11 flex items-center justify-center cursor-pointer rounded border border-zinc-700 px-2.5 text-xs text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors"
        >
          {importing ? "Importing…" : "Import .ics"}
          <input
            ref={fileInputRef}
            type="file"
            accept=".ics,text/calendar"
            onChange={handleImportIcs}
            disabled={importing}
            className="sr-only"
          />
        </label>
      </div>

      <button
        type="button"
        aria-label="Create event for today"
        onClick={() => {
          setYear(today.getUTCFullYear());
          setMonth(today.getUTCMonth());
          setSelectedDay(todayText);
          setSelectedEvent(null);
          setEditingEvent(null);
          setEditorOpen(true);
        }}
        className="min-h-11 whitespace-nowrap rounded bg-accent px-3 text-sm font-semibold text-zinc-900 hover:opacity-90 transition-opacity ml-1"
      >
        +<span className="hidden sm:inline"> New</span>
      </button>
    </>
  );

  return (
    <div className="flex h-dvh flex-col bg-zinc-900 text-zinc-200">
      <AppHeader homeHref={dashboardHref(runtime.shellContext)} appName="Calendar" embedded={embedded} />
      <div
        role="toolbar"
        aria-label="Calendar controls"
        className="flex shrink-0 items-center gap-1 border-b border-white/10 px-2 py-1 sm:px-4"
      >
        {navSlot}
      </div>

      <main className="flex min-h-0 flex-1 flex-col md:flex-row overflow-hidden">
        {/* Calendar grid according to viewMode */}
        <section aria-label="Calendar" className="flex min-h-0 min-w-0 flex-1 flex-col overflow-auto">
          {loading ? (
            <p className="p-6 text-zinc-400">Loading calendar…</p>
          ) : error ? (
            <div className="p-6">
              <p className="text-zinc-300">
                {error === "offline" ? "Calendar is offline."
                  : error === "validation" ? "Calendar request needs attention."
                  : error === "delete_failed" ? "Could not delete this event. Try again."
                  : "Calendar is unavailable."}
              </p>
              <button
                type="button"
                onClick={() => void load()}
                className="mt-3 rounded border border-zinc-700 px-3 py-1.5 text-sm hover:border-zinc-500 transition-colors"
              >
                Retry
              </button>
            </div>
          ) : (
            <>
              {events.length === 0 && (
                <p className="px-4 pt-3 text-sm text-zinc-500">No events this month.</p>
              )}
              {viewMode === "month" && (
                <MonthGrid
                  year={year}
                  month={month}
                  events={events}
                  selectedDay={selectedDay}
                  today={todayText}
                  onSelectDay={(day) => {
                    setSelectedDay(day);
                    setSelectedEvent(null);
                    setSharingOpen(false);
                    setEditingEvent(null);
                    setEditorOpen(false);
                  }}
                  onSelectEvent={(event) => {
                    setSelectedDay(event.startTime.slice(0, 10));
                    setSelectedEvent(event);
                    setEditorOpen(false);
                    setEditingEvent(null);
                    setSharingOpen(false);
                  }}
                />
              )}
              {viewMode === "week" && (
                <WeekGrid
                  currentDate={new Date(Date.UTC(year, month, selectedDay ? parseInt(selectedDay.slice(8, 10), 10) : today.getUTCDate()))}
                  events={events}
                  selectedDay={selectedDay}
                  today={todayText}
                  onSelectDay={(day) => {
                    setSelectedDay(day);
                    setSelectedEvent(null);
                    setSharingOpen(false);
                    setEditingEvent(null);
                    setEditorOpen(false);
                  }}
                  onSelectEvent={(event) => {
                    setSelectedDay(event.startTime.slice(0, 10));
                    setSelectedEvent(event);
                    setEditorOpen(false);
                    setEditingEvent(null);
                    setSharingOpen(false);
                  }}
                />
              )}
              {viewMode === "day" && (
                <DayGrid
                  day={selectedDay ?? todayText}
                  events={events}
                  today={todayText}
                  onSelectEvent={(event) => {
                    setSelectedDay(event.startTime.slice(0, 10));
                    setSelectedEvent(event);
                    setEditorOpen(false);
                    setEditingEvent(null);
                    setSharingOpen(false);
                  }}
                  onNewEventAtHour={(hour) => {
                    const targetDay = selectedDay ?? todayText;
                    setSelectedDay(targetDay);
                    setSelectedEvent(null);
                    setEditingEvent(null);
                    setEditorOpen(true);
                  }}
                />
              )}
            </>
          )}
        </section>

        {/* Day sidebar */}
        {selectedDay && !loading && !error && (
          <aside
            aria-label="Selected day"
            className="max-h-[45dvh] overflow-y-auto border-t border-zinc-700 bg-zinc-900 p-4 pb-[max(1rem,env(safe-area-inset-bottom))] md:max-h-none md:w-96 md:border-l md:border-t-0 md:overflow-y-auto"
          >
            <div className="mb-3 flex items-center justify-between">
              <h2 className="font-semibold text-zinc-100">{readableDay(selectedDay)}</h2>
              <button
                type="button"
                aria-label="Close selected day"
                onClick={closeAside}
                className="flex items-center justify-center rounded p-2 -m-1 min-h-[44px] min-w-[44px] text-zinc-500 hover:text-zinc-200 hover:bg-zinc-800 transition-colors"
              >
                ×
              </button>
            </div>

            {/* Editing an existing event */}
            {editingEvent ? (
              <EventEditor
                day={editingEvent.startTime.slice(0, 10)}
                event={editingEvent}
                onSave={saveEdit}
                onCancel={() => { setEditingEvent(null); setSelectedEvent(editingEvent); }}
              />
            ) : selectedEvent ? (
              /* Viewing a specific event */
              <>
                <EventDetails
                  event={selectedEvent}
                  onDelete={remove}
                  onEdit={() => { setEditingEvent(selectedEvent); setSelectedEvent(null); }}
                  onShare={() => setSharingOpen(true)}
                />
                {sharingOpen && (
                  <SharingPanel eventId={selectedEvent.id} api={api} runtime={runtime} />
                )}
              </>
            ) : (
              /* Day overview: list of events + create form */
              <>
                {selectedEvents.map((event) => (
                  <div
                    key={event.id}
                    className="mb-2"
                  >
                    <EventDetails
                      event={event}
                      onDelete={remove}
                      onSelect={() => setSelectedEvent(event)}
                      onEdit={() => setEditingEvent(event)}
                      onShare={() => { setSelectedEvent(event); setSharingOpen(true); }}
                    />
                  </div>
                ))}

                {!editorOpen && (
                  <button
                    type="button"
                    onClick={() => setEditorOpen(true)}
                    className="mt-2 w-full rounded border border-dashed border-zinc-700 py-2 text-sm text-zinc-400 hover:border-zinc-500 hover:text-zinc-200 transition-colors"
                  >
                    + Add event
                  </button>
                )}
                {editorOpen && (
                  <EventEditor
                    day={selectedDay}
                    onSave={saveNew}
                    onCancel={() => setEditorOpen(false)}
                  />
                )}
              </>
            )}
          </aside>
        )}
      </main>
    </div>
  );
}
