import { useCallback, useEffect, useMemo, useState } from "react";
import { AppHeader } from "../../../../packages/nexus-app-shell/src";
import type { CalendarApi, CalEvent, EventInput } from "./calendar-api";
import type { CalendarRuntime } from "./runtime";
import EventDetails from "./EventDetails";
import EventEditor from "./EventEditor";
import MonthGrid from "./MonthGrid";
import SharingPanel from "./SharingPanel";

const MONTHS = [
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
];

const datePart = (date: Date) => date.toISOString().slice(0, 10);

const rangeFor = (year: number, month: number) => ({
  from: `${year}-${String(month + 1).padStart(2, "0")}-01`,
  to: `${year}-${String(month + 1).padStart(2, "0")}-${String(new Date(Date.UTC(year, month + 1, 0)).getUTCDate()).padStart(2, "0")}`,
});

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
  const [events, setEvents] = useState<CalEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedDay, setSelectedDay] = useState<string | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<CalEvent | null>(null);
  const [editorOpen, setEditorOpen] = useState(false);
  const [editingEvent, setEditingEvent] = useState<CalEvent | null>(null);
  const [sharingOpen, setSharingOpen] = useState(false);

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

  const selectedEvents = selectedDay
    ? events.filter((event) =>
        event.startTime.slice(0, 10) === selectedDay ||
        (Date.parse(event.startTime) < Date.parse(`${selectedDay}T23:59:59Z`) &&
          Date.parse(event.endTime) > Date.parse(`${selectedDay}T00:00:00Z`)),
      )
    : [];

  const navSlot = (
    <>
      <button
        type="button"
        aria-label="Previous month"
        onClick={() => changeMonth(-1)}
        className="rounded px-2 py-1 text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 transition-colors"
      >
        ←
      </button>
      <span aria-live="polite" className="text-center text-sm text-zinc-200 min-w-0 sm:min-w-32">
        {MONTHS[month]} {year}
      </span>
      <button
        type="button"
        aria-label="Next month"
        onClick={() => changeMonth(1)}
        className="rounded px-2 py-1 text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 transition-colors"
      >
        →
      </button>
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
        className="whitespace-nowrap rounded bg-accent px-3 py-1.5 text-sm font-semibold text-zinc-900 hover:opacity-90 transition-opacity"
      >
        + New
      </button>
    </>
  );

  return (
    <div className="flex min-h-screen flex-col bg-zinc-900 text-zinc-200">
      <AppHeader homeHref="/" appName="Calendar" utilitySlot={navSlot} />

      <main className="flex min-h-0 flex-1 flex-col md:flex-row overflow-hidden">
        {/* Calendar grid */}
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
            </>
          )}
        </section>

        {/* Day sidebar */}
        {selectedDay && !loading && !error && (
          <aside
            aria-label="Selected day"
            className="max-h-[40vh] overflow-y-auto border-t border-zinc-700 bg-zinc-900 p-4 md:max-h-none md:w-96 md:border-l md:border-t-0 md:overflow-y-auto"
          >
            <div className="mb-3 flex items-center justify-between">
              <h2 className="font-semibold text-zinc-100">{readableDay(selectedDay)}</h2>
              <button
                type="button"
                aria-label="Close selected day"
                onClick={closeAside}
                className="text-zinc-500 hover:text-zinc-200 transition-colors"
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
                    role="button"
                    tabIndex={0}
                    aria-label={event.title}
                    onClick={() => setSelectedEvent(event)}
                    onKeyDown={e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); setSelectedEvent(event); } }}
                    className="mb-2 cursor-pointer"
                  >
                    <EventDetails
                      event={event}
                      onDelete={remove}
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
