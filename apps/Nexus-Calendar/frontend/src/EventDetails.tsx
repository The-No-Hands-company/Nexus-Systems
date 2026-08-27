import type { CalEvent } from "./calendar-api";

export default function EventDetails({ event, onDelete, onShare }: { event: CalEvent; onDelete(): Promise<void>; onShare(): void }) {
  const canEdit = event.access === "owner" || event.access === "editor";
  return <article className="rounded border border-zinc-800 bg-zinc-900/50 p-3">
    <h3 className="font-medium">{event.title}</h3>
    <p className="mt-1 text-sm text-zinc-400">{event.allDay ? "All day" : `${event.startTime.slice(11, 16)} – ${event.endTime.slice(11, 16)}`}</p>
    {event.location && <p className="text-sm text-zinc-400">{event.location}</p>}
    {event.description && <p className="mt-2 text-sm text-zinc-300">{event.description}</p>}
    <p className="mt-2 text-xs uppercase tracking-wide text-zinc-500">{event.access} access</p>
    <div className="mt-3 flex gap-2">
      {event.access === "owner" && <button type="button" onClick={onShare} className="rounded border border-zinc-700 px-2 py-1 text-sm">Manage sharing</button>}
      {canEdit && event.access === "owner" && <button type="button" onClick={() => void onDelete()} className="rounded px-2 py-1 text-sm text-red-300 hover:bg-red-400/10">Delete event</button>}
    </div>
  </article>;
}
