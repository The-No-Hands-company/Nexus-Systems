import { useEffect, useState } from "react";
import { calendarApi, type CalendarApi, type PublicEvent } from "./calendar-api";

export default function PublicEventPage({ token, api = calendarApi("/api/v1/calendar") }: { token: string; api?: CalendarApi }) {
  const [event, setEvent] = useState<PublicEvent | null>(null); const [missing, setMissing] = useState(false);
  useEffect(() => { (document as Document & { referrerPolicy: string }).referrerPolicy = "no-referrer"; void api.getPublicEvent(token).then(setEvent).catch(() => setMissing(true)); }, [api, token]);
  if (missing) return <main className="grid min-h-screen place-items-center bg-[#070707] p-6 text-zinc-300"><p>This event is unavailable.</p></main>;
  if (!event) return <main className="grid min-h-screen place-items-center bg-[#070707] p-6 text-zinc-400">Loading event…</main>;
  return <main className="grid min-h-screen place-items-center bg-[#070707] p-6 text-white"><article className="w-full max-w-lg rounded-xl border border-zinc-800 bg-zinc-950 p-6"><p className="text-sm text-zinc-500">Shared event</p><h1 className="mt-1 text-2xl font-semibold">{event.title}</h1><p className="mt-4 text-zinc-300">{event.allDay ? "All day" : `${new Date(event.startTime).toLocaleString()} – ${new Date(event.endTime).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}`}</p>{event.location && <p className="mt-2 text-zinc-300">{event.location}</p>}{event.description && <p className="mt-4 whitespace-pre-wrap text-zinc-400">{event.description}</p>}</article></main>;
}
