import { useState, useEffect } from "react";
import {
  listCalendarEvents,
  createCalendarEvent,
  deleteCalendarEvent,
  ApiError,
  type CalendarEvent,
  type CalendarEventInput,
} from "../../api";

// ── date helpers ─────────────────────────────────────────────────────────────

function toDateKey(d: Date): string {
  return d.toISOString().slice(0, 10);
}

function monthLabel(d: Date): string {
  return d.toLocaleDateString("en-GB", { month: "long", year: "numeric" });
}

function dayLabel(key: string): string {
  return new Date(`${key}T12:00:00`).toLocaleDateString("en-GB", {
    weekday: "long",
    day: "numeric",
    month: "long",
  });
}

function addMonths(d: Date, n: number): Date {
  const r = new Date(d);
  r.setDate(1);
  r.setMonth(r.getMonth() + n);
  return r;
}

/** 6-row × 7-col grid, Monday-first. */
function buildGrid(year: number, month: number): Date[] {
  const startOffset = (new Date(year, month, 1).getDay() + 6) % 7;
  return Array.from({ length: 42 }, (_, i) => new Date(year, month, 1 - startOffset + i));
}

function eventTime(e: CalendarEvent): string {
  if (e.allDay) return "All day";
  const t = e.startTime.slice(11, 16);
  return t || "";
}

// ── types ─────────────────────────────────────────────────────────────────────

type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; events: CalendarEvent[] };

// ── main component ────────────────────────────────────────────────────────────

const DAYS = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"];

export default function CalendarView() {
  const today = toDateKey(new Date());

  const [viewDate, setViewDate] = useState<Date>(() => {
    const d = new Date();
    d.setDate(1);
    return d;
  });
  const [selectedDate, setSelectedDate] = useState(today);
  const [loadState, setLoadState] = useState<LoadState>({ kind: "loading" });
  const [showForm, setShowForm] = useState(false);
  const [submitting, setSubmitting] = useState(false);

  const year = viewDate.getFullYear();
  const month = viewDate.getMonth();
  const grid = buildGrid(year, month);
  const from = toDateKey(grid[0]!);
  const to = toDateKey(grid[grid.length - 1]!);

  useEffect(() => {
    let cancelled = false;
    setLoadState({ kind: "loading" });
    listCalendarEvents(from, to)
      .then((events) => {
        if (!cancelled) setLoadState({ kind: "ready", events });
      })
      .catch((e) => {
        if (!cancelled)
          setLoadState({
            kind: "error",
            message: e instanceof ApiError ? e.message : "Calendar unavailable",
          });
      });
    return () => {
      cancelled = true;
    };
  }, [from, to]);

  function eventsOn(date: string): CalendarEvent[] {
    if (loadState.kind !== "ready") return [];
    return loadState.events.filter((e) => e.startTime.slice(0, 10) === date);
  }

  async function handleDelete(id: string) {
    await deleteCalendarEvent(id);
    setLoadState((prev) => {
      if (prev.kind !== "ready") return prev;
      return { kind: "ready", events: prev.events.filter((e) => e.id !== id) };
    });
  }

  async function handleCreate(input: CalendarEventInput) {
    setSubmitting(true);
    try {
      const event = await createCalendarEvent(input);
      setLoadState((prev) => {
        if (prev.kind !== "ready") return prev;
        return { kind: "ready", events: [...prev.events, event] };
      });
      setShowForm(false);
    } finally {
      setSubmitting(false);
    }
  }

  const selectedEvents = eventsOn(selectedDate);
  const todayEvents = eventsOn(today);

  return (
    <section className="mx-auto max-w-5xl px-4 py-6 sm:p-8">
      {/* ── header ── */}
      <div className="mb-6 flex flex-wrap items-center justify-between gap-4">
        <div className="flex items-center gap-2">
          <button
            type="button"
            onClick={() => setViewDate((d) => addMonths(d, -1))}
            className="rounded border border-zinc-700 px-3 py-1.5 text-sm hover:bg-zinc-800"
            aria-label="Previous month"
          >
            ‹
          </button>
          <h1 className="w-44 text-center font-semibold" aria-live="polite">
            {monthLabel(viewDate)}
          </h1>
          <button
            type="button"
            onClick={() => setViewDate((d) => addMonths(d, 1))}
            className="rounded border border-zinc-700 px-3 py-1.5 text-sm hover:bg-zinc-800"
            aria-label="Next month"
          >
            ›
          </button>
        </div>
        <button
          type="button"
          onClick={() => setShowForm(true)}
          className="rounded bg-accent px-4 py-1.5 text-sm font-medium text-accent-foreground hover:bg-accent-hover"
        >
          + New event
        </button>
      </div>

      <div className="flex flex-col gap-6 lg:flex-row lg:items-start">
        {/* ── month grid ── */}
        <div className="w-full lg:w-80 xl:w-96 shrink-0">
          <div className="overflow-hidden rounded-lg border border-zinc-700 bg-zinc-700 gap-px grid grid-cols-7">
            {DAYS.map((d) => (
              <div
                key={d}
                className="bg-zinc-900 py-1.5 text-center font-mono text-[10px] uppercase tracking-widest text-zinc-500"
              >
                {d}
              </div>
            ))}
            {grid.map((date) => {
              const key = toDateKey(date);
              const inMonth = date.getMonth() === month;
              const isToday = key === today;
              const isSel = key === selectedDate;
              const hasDot = loadState.kind === "ready" && eventsOn(key).length > 0;

              return (
                <button
                  key={key}
                  type="button"
                  onClick={() => setSelectedDate(key)}
                  aria-label={key}
                  aria-pressed={isSel}
                  className={[
                    "relative flex h-10 flex-col items-center justify-center gap-0.5 text-sm transition-colors",
                    "focus:outline-none focus-visible:ring-1 focus-visible:ring-accent",
                    isSel
                      ? "bg-accent text-accent-foreground"
                      : isToday
                        ? "bg-zinc-900 text-zinc-100 ring-1 ring-inset ring-accent"
                        : inMonth
                          ? "bg-zinc-900 hover:bg-zinc-800"
                          : "bg-zinc-900/60 text-zinc-600 hover:bg-zinc-800/60",
                  ]
                    .filter(Boolean)
                    .join(" ")}
                >
                  {date.getDate()}
                  {hasDot && !isSel && (
                    <span className="h-1 w-1 rounded-full bg-accent opacity-80" />
                  )}
                </button>
              );
            })}
          </div>
        </div>

        {/* ── right panel ── */}
        <div className="flex-1 min-w-0 space-y-4">
          {showForm ? (
            <NewEventForm
              initialDate={selectedDate}
              onCancel={() => setShowForm(false)}
              onSubmit={handleCreate}
              submitting={submitting}
            />
          ) : (
            <EventsPanel
              date={selectedDate}
              events={selectedEvents}
              loadState={loadState}
              onDelete={handleDelete}
              onNew={() => setShowForm(true)}
            />
          )}

          {/* Today strip — only when viewing a different day */}
          {selectedDate !== today && todayEvents.length > 0 && (
            <div className="rounded-lg border border-zinc-700 bg-zinc-800/50 p-4">
              <p className="mb-3 font-mono text-[10px] uppercase tracking-widest text-zinc-500">
                Today
              </p>
              <ul className="space-y-1">
                {todayEvents.map((e) => (
                  <li key={e.id} className="flex items-baseline gap-3">
                    <span className="w-12 shrink-0 font-mono text-xs text-zinc-400">
                      {eventTime(e)}
                    </span>
                    <span className="text-sm">{e.title}</span>
                  </li>
                ))}
              </ul>
            </div>
          )}
        </div>
      </div>
    </section>
  );
}

// ── EventsPanel ───────────────────────────────────────────────────────────────

function EventsPanel({
  date,
  events,
  loadState,
  onDelete,
  onNew,
}: {
  date: string;
  events: CalendarEvent[];
  loadState: LoadState;
  onDelete: (id: string) => void;
  onNew: () => void;
}) {
  return (
    <div className="rounded-lg border border-zinc-700 bg-zinc-800 p-4">
      <div className="mb-4 flex items-center justify-between gap-2">
        <h2 className="font-semibold leading-snug">{dayLabel(date)}</h2>
        <button
          type="button"
          onClick={onNew}
          className="shrink-0 text-sm text-accent hover:underline"
        >
          + New
        </button>
      </div>

      {loadState.kind === "loading" && (
        <p className="py-4 text-sm text-zinc-500">Loading…</p>
      )}
      {loadState.kind === "error" && (
        <p role="alert" className="py-4 text-sm text-state-danger">
          {loadState.message}
        </p>
      )}
      {loadState.kind === "ready" && events.length === 0 && (
        <p className="py-4 text-center text-sm text-zinc-500">Nothing scheduled</p>
      )}
      {loadState.kind === "ready" && events.length > 0 && (
        <ul className="space-y-2">
          {events.map((e) => (
            <li
              key={e.id}
              className="group flex items-center gap-3 rounded border border-zinc-700 bg-zinc-900/50 px-3 py-2"
            >
              <span className="w-12 shrink-0 font-mono text-xs text-zinc-400">
                {eventTime(e)}
              </span>
              <span className="flex-1 min-w-0 text-sm truncate">{e.title}</span>
              <button
                type="button"
                onClick={() => onDelete(e.id)}
                className="shrink-0 text-xs text-zinc-600 opacity-0 transition-opacity group-hover:opacity-100 hover:text-state-danger focus-visible:opacity-100"
                aria-label={`Delete ${e.title}`}
              >
                ×
              </button>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

// ── NewEventForm ──────────────────────────────────────────────────────────────

function NewEventForm({
  initialDate,
  onCancel,
  onSubmit,
  submitting,
}: {
  initialDate: string;
  onCancel: () => void;
  onSubmit: (input: CalendarEventInput) => Promise<void>;
  submitting: boolean;
}) {
  const [title, setTitle] = useState("");
  const [date, setDate] = useState(initialDate);
  const [startTime, setStartTime] = useState("");
  const [endTime, setEndTime] = useState("");
  const [allDay, setAllDay] = useState(false);
  const [description, setDescription] = useState("");
  const [formError, setFormError] = useState<string | null>(null);

  async function handleSubmit(ev: React.FormEvent) {
    ev.preventDefault();
    if (!title.trim()) {
      setFormError("Title is required");
      return;
    }
    setFormError(null);
    const startIso = allDay
      ? `${date}T00:00:00Z`
      : `${date}T${startTime || "00:00"}:00Z`;
    const input: CalendarEventInput = {
      title: title.trim(),
      startTime: startIso,
      allDay,
      ...(endTime && !allDay ? { endTime: `${date}T${endTime}:00Z` } : {}),
      ...(description.trim() ? { description: description.trim() } : {}),
    };
    try {
      await onSubmit(input);
    } catch (e) {
      setFormError(e instanceof ApiError ? e.message : "Failed to create event");
    }
  }

  return (
    <div className="rounded-lg border border-zinc-700 bg-zinc-800 p-4">
      <h2 className="mb-4 font-semibold">New event</h2>
      <form onSubmit={handleSubmit} className="space-y-3">
        <div>
          <label htmlFor="cal-title" className="mb-1 block text-xs text-zinc-400">
            Title
          </label>
          <input
            id="cal-title"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            placeholder="Event title"
            // biome-ignore lint/a11y/noAutofocus: form opens intentionally
            autoFocus
            className="w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-sm focus:border-accent focus:outline-none"
          />
        </div>

        <div>
          <label htmlFor="cal-date" className="mb-1 block text-xs text-zinc-400">
            Date
          </label>
          <input
            id="cal-date"
            type="date"
            value={date}
            onChange={(e) => setDate(e.target.value)}
            className="rounded border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-sm focus:border-accent focus:outline-none"
          />
        </div>

        <div className="flex items-center gap-2">
          <input
            id="cal-allday"
            type="checkbox"
            checked={allDay}
            onChange={(e) => setAllDay(e.target.checked)}
            className="accent-accent"
          />
          <label htmlFor="cal-allday" className="text-sm">
            All day
          </label>
        </div>

        {!allDay && (
          <div className="flex gap-3">
            <div>
              <label htmlFor="cal-start" className="mb-1 block text-xs text-zinc-400">
                Start
              </label>
              <input
                id="cal-start"
                type="time"
                value={startTime}
                onChange={(e) => setStartTime(e.target.value)}
                className="rounded border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-sm focus:border-accent focus:outline-none"
              />
            </div>
            <div>
              <label htmlFor="cal-end" className="mb-1 block text-xs text-zinc-400">
                End
              </label>
              <input
                id="cal-end"
                type="time"
                value={endTime}
                onChange={(e) => setEndTime(e.target.value)}
                className="rounded border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-sm focus:border-accent focus:outline-none"
              />
            </div>
          </div>
        )}

        <div>
          <label htmlFor="cal-desc" className="mb-1 block text-xs text-zinc-400">
            Description (optional)
          </label>
          <textarea
            id="cal-desc"
            value={description}
            onChange={(e) => setDescription(e.target.value)}
            rows={2}
            className="w-full resize-none rounded border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-sm focus:border-accent focus:outline-none"
          />
        </div>

        {formError && (
          <p role="alert" className="text-sm text-state-danger">
            {formError}
          </p>
        )}

        <div className="flex justify-end gap-2 pt-1">
          <button
            type="button"
            onClick={onCancel}
            disabled={submitting}
            className="rounded border border-zinc-700 px-4 py-1.5 text-sm hover:bg-zinc-700 disabled:opacity-50"
          >
            Cancel
          </button>
          <button
            type="submit"
            disabled={submitting}
            className="rounded bg-accent px-4 py-1.5 text-sm font-medium text-accent-foreground hover:bg-accent-hover disabled:opacity-50"
          >
            {submitting ? "Saving…" : "Save"}
          </button>
        </div>
      </form>
    </div>
  );
}
