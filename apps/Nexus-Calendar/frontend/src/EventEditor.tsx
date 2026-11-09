import { useState, type FormEvent } from "react";
import type { CalEvent, EventInput } from "./calendar-api";
import { parseCategory, cleanDescription, parseReminder, CATEGORIES, type EventCategory } from "./categories";

const inputClass =
  "mt-1 w-full rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus-visible:border-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent/60 focus-visible:ring-offset-1 focus-visible:ring-offset-zinc-800";

export default function EventEditor({
  day,
  event,
  onSave,
  onCancel,
}: {
  day: string;
  event?: CalEvent;
  onSave(input: EventInput): Promise<void>;
  onCancel(): void;
}) {
  const [title, setTitle] = useState(event?.title ?? "");
  const [start, setStart] = useState(event?.startTime.slice(11, 16) ?? "09:00");
  const [end, setEnd] = useState(event?.endTime.slice(11, 16) ?? "10:00");
  const [allDay, setAllDay] = useState(event?.allDay ?? false);
  const [recurrence, setRecurrence] = useState(event?.recurrence ?? "none");
  const [recurrenceScope, setRecurrenceScope] = useState<"this" | "series">("series");
  const [category, setCategory] = useState<EventCategory>(parseCategory(event?.description));
  const [reminder, setReminder] = useState(parseReminder(event?.description));
  const [location, setLocation] = useState(event?.location ?? "");
  const [description, setDescription] = useState(cleanDescription(event?.description));
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);

  const editing = !!event;
  const isRecurring = Boolean(event?.recurrence && event.recurrence !== "none");

  async function submit(e: FormEvent) {
    e.preventDefault();
    setSaving(true);
    setError(null);
    try {
      const startTime = allDay ? `${day}T00:00:00.000Z` : `${day}T${start}:00.000Z`;
      const endTime = allDay ? `${day}T23:59:59.000Z` : `${day}T${end}:00.000Z`;
      const recurrenceValue =
        recurrence === "none"
          ? undefined
          : recurrenceScope === "this"
            ? undefined
            : recurrence;

      let finalDesc = description.trim();
      if (category !== "general") finalDesc = `${finalDesc} [category:${category}]`.trim();
      if (reminder !== "none") finalDesc = `${finalDesc} [reminder:${reminder}]`.trim();

      await onSave({
        title,
        location: location || undefined,
        description: finalDesc || undefined,
        startTime,
        endTime,
        allDay,
        recurrence: recurrenceValue,
      });
    } catch (reason) {
      setError(
        (reason as { reason?: string }).reason === "validation"
          ? "Check the event details and try again."
          : "Could not save this event. Try again.",
      );
    } finally {
      setSaving(false);
    }
  }

  return (
    <form onSubmit={submit} className="space-y-3 pt-3" aria-label="Event editor">
      {error && <p role="alert" className="text-sm text-state-danger">{error}</p>}

      <label className="block text-sm text-zinc-300">
        Title
        <input
          aria-label="Title"
          required
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          className={inputClass}
        />
      </label>

      {/* All Day Toggle */}
      <label className="flex items-center gap-2 text-xs text-zinc-300 cursor-pointer">
        <input
          type="checkbox"
          checked={allDay}
          onChange={(e) => setAllDay(e.target.checked)}
          className="rounded border-zinc-700 bg-zinc-800 text-accent focus:ring-accent"
        />
        <span>All day event</span>
      </label>

      {!allDay && (
        <div className="flex gap-2">
          <label className="text-sm text-zinc-300 flex-1">
            Start
            <input
              aria-label="Start time"
              type="time"
              value={start}
              onChange={(e) => setStart(e.target.value)}
              className="mt-1 block w-full rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus-visible:border-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent/60 focus-visible:ring-offset-1 focus-visible:ring-offset-zinc-800"
            />
          </label>
          <label className="text-sm text-zinc-300 flex-1">
            End
            <input
              aria-label="End time"
              type="time"
              value={end}
              onChange={(e) => setEnd(e.target.value)}
              className="mt-1 block w-full rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus-visible:border-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent/60 focus-visible:ring-offset-1 focus-visible:ring-offset-zinc-800"
            />
          </label>
        </div>
      )}

      {/* Recurrence Rule */}
      <label className="block text-sm text-zinc-300">
        Repeats
        <select
          aria-label="Recurrence"
          value={recurrence}
          onChange={(e) => setRecurrence(e.target.value)}
          className={inputClass}
        >
          <option value="none">Does not repeat</option>
          <option value="RRULE:FREQ=DAILY">Daily</option>
          <option value="RRULE:FREQ=WEEKLY">Weekly</option>
          <option value="RRULE:FREQ=MONTHLY">Monthly</option>
          <option value="RRULE:FREQ=YEARLY">Yearly</option>
        </select>
      </label>

      {/* Recurring Edit Scope Notice */}
      {editing && isRecurring && (
        <div className="rounded-md border border-zinc-700 bg-zinc-900/80 p-2.5 text-xs text-zinc-300 space-y-1.5">
          <span className="font-semibold text-zinc-200 block">Edit Recurring Event:</span>
          <div className="flex gap-3">
            <label className="flex items-center gap-1.5 cursor-pointer">
              <input
                type="radio"
                name="recurrenceScope"
                value="this"
                checked={recurrenceScope === "this"}
                onChange={() => setRecurrenceScope("this")}
                className="text-accent focus:ring-accent"
              />
              <span>Only this event</span>
            </label>
            <label className="flex items-center gap-1.5 cursor-pointer">
              <input
                type="radio"
                name="recurrenceScope"
                value="series"
                checked={recurrenceScope === "series"}
                onChange={() => setRecurrenceScope("series")}
                className="text-accent focus:ring-accent"
              />
              <span>All future events</span>
            </label>
          </div>
        </div>
      )}

      {/* Category & Reminder Controls */}
      <div className="grid grid-cols-2 gap-2">
        <label className="block text-sm text-zinc-300">
          Category
          <select
            aria-label="Category"
            value={category}
            onChange={(e) => setCategory(e.target.value as EventCategory)}
            className={inputClass}
          >
            {Object.values(CATEGORIES).map((c) => (
              <option key={c.id} value={c.id}>
                {c.label}
              </option>
            ))}
          </select>
        </label>

        <label className="block text-sm text-zinc-300">
          Reminder
          <select
            aria-label="Reminder"
            value={reminder}
            onChange={(e) => setReminder(e.target.value)}
            className={inputClass}
          >
            <option value="none">No reminder</option>
            <option value="5m">5 minutes before</option>
            <option value="15m">15 minutes before</option>
            <option value="30m">30 minutes before</option>
            <option value="1h">1 hour before</option>
            <option value="1d">1 day before</option>
          </select>
        </label>
      </div>

      <label className="block text-sm text-zinc-300">
        Location
        <input
          aria-label="Location"
          value={location}
          onChange={(e) => setLocation(e.target.value)}
          placeholder="Room, office, or meeting link"
          className={inputClass}
        />
      </label>

      <label className="block text-sm text-zinc-300">
        Description
        <textarea
          aria-label="Description"
          value={description}
          onChange={(e) => setDescription(e.target.value)}
          rows={3}
          placeholder="Notes or agenda"
          className={inputClass}
        />
      </label>

      <div className="flex gap-2 pt-1">
        <button
          type="submit"
          disabled={saving}
          className="rounded bg-accent px-3 py-1.5 text-sm font-medium text-zinc-900 hover:opacity-90 disabled:opacity-50 transition-opacity"
        >
          {saving ? "Saving…" : editing ? "Update event" : "Save event"}
        </button>
        <button
          type="button"
          onClick={onCancel}
          className="rounded border border-zinc-700 px-3 py-1.5 text-sm text-zinc-300 hover:text-zinc-100 transition-colors"
        >
          Cancel
        </button>
      </div>
    </form>
  );
}
