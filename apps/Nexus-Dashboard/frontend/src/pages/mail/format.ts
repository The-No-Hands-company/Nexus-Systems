/** Short, human date for a message list. */
export function mailDate(iso: string): string {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return "";
  const now = new Date();
  const sameDay = d.toDateString() === now.toDateString();
  return sameDay
    ? d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" })
    : d.toLocaleDateString(undefined, { month: "short", day: "numeric" });
}

/**
 * The display name from an address header, falling back to the address.
 * `Alice <alice@tnhc.dev>` reads better as `Alice` in a list.
 */
export function senderName(from: string): string {
  const named = from.match(/^\s*"?([^"<]+?)"?\s*<[^>]+>\s*$/);
  return named ? named[1].trim() : from.trim();
}
