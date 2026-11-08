import type { CalEvent, EventInput } from "./calendar-api";

function formatIcsDate(isoString: string): string {
  const d = new Date(isoString);
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getUTCFullYear()}${pad(d.getUTCMonth() + 1)}${pad(d.getUTCDate())}T${pad(d.getUTCHours())}${pad(d.getUTCMinutes())}${pad(d.getUTCSeconds())}Z`;
}

function parseIcsDate(icsDateStr: string): string {
  // Supports YYYYMMDDTHHMMSSZ or YYYYMMDD
  const clean = icsDateStr.replace(/[^0-9TZ]/g, "");
  if (clean.length === 8) {
    const y = clean.slice(0, 4);
    const m = clean.slice(4, 6);
    const d = clean.slice(6, 8);
    return `${y}-${m}-${d}T00:00:00.000Z`;
  }
  if (clean.length >= 15) {
    const y = clean.slice(0, 4);
    const m = clean.slice(4, 6);
    const d = clean.slice(6, 8);
    const h = clean.slice(9, 11);
    const min = clean.slice(11, 13);
    const s = clean.slice(13, 15);
    return `${y}-${m}-${d}T${h}:${min}:${s}.000Z`;
  }
  return new Date().toISOString();
}

/**
 * Serializes calendar events into RFC 5545 iCalendar (.ics) format.
 * Compatible with Apple Calendar, Google Calendar, Outlook, and Thunderbird.
 */
export function exportEventsToIcs(events: CalEvent[], calendarName = "Nexus Calendar"): string {
  const now = formatIcsDate(new Date().toISOString());
  const lines: string[] = [
    "BEGIN:VCALENDAR",
    "VERSION:2.0",
    "PRODID:-//The No Hands Company//Nexus Calendar 1.0//EN",
    "CALSCALE:GREGORIAN",
    "METHOD:PUBLISH",
    `X-WR-CALNAME:${calendarName}`,
  ];

  for (const ev of events) {
    lines.push("BEGIN:VEVENT");
    lines.push(`UID:${ev.id || crypto.randomUUID()}@tnhc.dev`);
    lines.push(`DTSTAMP:${now}`);
    lines.push(`DTSTART:${formatIcsDate(ev.startTime)}`);
    lines.push(`DTEND:${formatIcsDate(ev.endTime)}`);
    lines.push(`SUMMARY:${ev.title.replace(/\n/g, " ")}`);
    if (ev.description) {
      lines.push(`DESCRIPTION:${ev.description.replace(/\n/g, "\\n")}`);
    }
    if (ev.location) {
      lines.push(`LOCATION:${ev.location.replace(/\n/g, " ")}`);
    }
    if (ev.recurrence) {
      const rrule = ev.recurrence.startsWith("RRULE:") ? ev.recurrence.slice(6) : ev.recurrence;
      lines.push(`RRULE:${rrule}`);
    }
    lines.push("END:VEVENT");
  }

  lines.push("END:VCALENDAR");
  return lines.join("\r\n");
}

/**
 * Parses an RFC 5545 iCalendar string into an array of EventInputs.
 */
export function parseIcsToEvents(icsData: string): EventInput[] {
  const events: EventInput[] = [];
  // Unfold lines (RFC 5545 line continuation begins with whitespace)
  const unfolded = icsData.replace(/\r\n[ \t]/g, "").replace(/\n[ \t]/g, "");
  const veventBlocks = unfolded.split(/BEGIN:VEVENT/i).slice(1);

  for (const block of veventBlocks) {
    const rawBlock = block.split(/END:VEVENT/i)[0];
    const lines = rawBlock.split(/\r?\n/);
    let title = "Imported Event";
    let startTime = "";
    let endTime = "";
    let description: string | undefined;
    let location: string | undefined;
    let recurrence: string | undefined;

    for (const line of lines) {
      const colonIdx = line.indexOf(":");
      if (colonIdx === -1) continue;
      const keyPart = line.slice(0, colonIdx).toUpperCase();
      const val = line.slice(colonIdx + 1).trim();

      if (keyPart === "SUMMARY") {
        title = val;
      } else if (keyPart.startsWith("DTSTART")) {
        startTime = parseIcsDate(val);
      } else if (keyPart.startsWith("DTEND")) {
        endTime = parseIcsDate(val);
      } else if (keyPart === "DESCRIPTION") {
        description = val.replace(/\\n/g, "\n");
      } else if (keyPart === "LOCATION") {
        location = val;
      } else if (keyPart === "RRULE") {
        recurrence = `RRULE:${val}`;
      }
    }

    if (!startTime) {
      startTime = new Date().toISOString();
    }
    if (!endTime) {
      const startMs = new Date(startTime).getTime();
      endTime = new Date(startMs + 3600_000).toISOString();
    }

    events.push({
      title,
      startTime,
      endTime,
      location,
      description,
      recurrence,
    });
  }

  return events;
}
