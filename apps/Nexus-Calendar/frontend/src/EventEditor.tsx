import { useState, type FormEvent } from "react";
import type { CalEvent, EventInput } from "./calendar-api";

const inputClass = "mt-1 w-full rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus:border-zinc-500 focus:outline-none";

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
  const [location, setLocation] = useState(event?.location ?? "");
  const [description, setDescription] = useState(event?.description ?? "");
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);

  const editing = !!event;

  async function submit(e: FormEvent) {
    e.preventDefault();
    setSaving(true);
    setError(null);
    try {
      await onSave({
        title,
        location: location || undefined,
        description: description || undefined,
        startTime: `${day}T${start}:00.000Z`,
        endTime: `${day}T${end}:00.000Z`,
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

      <div className="flex gap-2">
        <label className="text-sm text-zinc-300">
          Start
          <input
            aria-label="Start time"
            type="time"
            value={start}
            onChange={(e) => setStart(e.target.value)}
            className="mt-1 block rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus:border-zinc-500 focus:outline-none"
          />
        </label>
        <label className="text-sm text-zinc-300">
          End
          <input
            aria-label="End time"
            type="time"
            value={end}
            onChange={(e) => setEnd(e.target.value)}
            className="mt-1 block rounded border border-zinc-700 bg-zinc-800 px-2 py-1.5 text-sm text-zinc-100 focus:border-zinc-500 focus:outline-none"
          />
        </label>
      </div>

      <label className="block text-sm text-zinc-300">
        Location
        <input
          aria-label="Location"
          value={location}
          onChange={(e) => setLocation(e.target.value)}
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
          className={inputClass}
        />
      </label>

      <div className="flex gap-2">
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
