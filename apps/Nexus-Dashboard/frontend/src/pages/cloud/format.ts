/**
 * Small formatting helpers shared by the ported Cloud views.
 *
 * `timeAgo` mirrors status.html's helper of the same name exactly (same
 * buckets, same thresholds) so relative timestamps read the same as they did
 * in Cloud's own console.
 */
export function timeAgo(ts?: string): string {
  if (!ts) return "—";
  const diff = Date.now() - new Date(ts).getTime();
  if (Number.isNaN(diff)) return "—";
  if (diff < 60_000) return `${Math.floor(diff / 1000)}s ago`;
  if (diff < 3_600_000) return `${Math.floor(diff / 60_000)}m ago`;
  if (diff < 86_400_000) return `${Math.floor(diff / 3_600_000)}h ago`;
  return `${Math.floor(diff / 86_400_000)}d ago`;
}

/** status.html's .status-dot.{healthy,degraded,offline,unknown} colours. */
export function healthDotClass(health?: string): string {
  switch (health) {
    case "healthy":
      return "bg-green-500";
    case "degraded":
      return "bg-amber-500";
    case "offline":
      return "bg-red-500";
    default:
      return "bg-zinc-500";
  }
}

export function healthTextClass(health?: string): string {
  switch (health) {
    case "healthy":
      return "text-green-400";
    case "degraded":
      return "text-amber-400";
    case "offline":
      return "text-red-400";
    default:
      return "text-zinc-500";
  }
}
