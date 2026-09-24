import { useState, type FormEvent } from "react";
import type { EventInput } from "./calendar-api";

export default function EventEditor({ day, onSave, onCancel }: { day: string; onSave(input: EventInput): Promise<void>; onCancel(): void }) {
  const [title, setTitle] = useState(""); const [start, setStart] = useState("09:00"); const [end, setEnd] = useState("10:00");
  const [location, setLocation] = useState(""); const [description, setDescription] = useState(""); const [error, setError] = useState<string | null>(null); const [saving, setSaving] = useState(false);
  async function submit(event: FormEvent) {
    event.preventDefault(); setSaving(true); setError(null);
    try { await onSave({ title, location: location || undefined, description: description || undefined, startTime: `${day}T${start}:00.000Z`, endTime: `${day}T${end}:00.000Z` }); }
    catch (reason) { setError((reason as { reason?: string }).reason === "validation" ? "Check the event details and try again." : "Could not save this event. Try again."); }
    finally { setSaving(false); }
  }
  return <form onSubmit={submit} className="space-y-3" aria-label="Event editor">
    {error && <p role="alert" className="text-sm text-red-300">{error}</p>}
    <label className="block text-sm">Title<input aria-label="Title" required value={title} onChange={(e) => setTitle(e.target.value)} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5" /></label>
    <div className="flex gap-2"><label className="text-sm">Start<input aria-label="Start time" type="time" value={start} onChange={(e) => setStart(e.target.value)} className="mt-1 block rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5" /></label><label className="text-sm">End<input aria-label="End time" type="time" value={end} onChange={(e) => setEnd(e.target.value)} className="mt-1 block rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5" /></label></div>
    <label className="block text-sm">Location<input aria-label="Location" value={location} onChange={(e) => setLocation(e.target.value)} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5" /></label>
    <label className="block text-sm">Description<textarea aria-label="Description" value={description} onChange={(e) => setDescription(e.target.value)} className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5" /></label>
    <div className="flex gap-2"><button type="submit" disabled={saving} className="rounded bg-[#9bd400] px-3 py-1.5 text-sm font-medium text-black">{saving ? "Saving…" : "Save event"}</button><button type="button" onClick={onCancel} className="rounded border border-zinc-700 px-3 py-1.5 text-sm">Cancel</button></div>
  </form>;
}
