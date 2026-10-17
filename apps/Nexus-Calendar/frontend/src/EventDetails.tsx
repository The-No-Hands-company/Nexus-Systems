import { Badge, type BadgeVariant } from "../../../../packages/nexus-design/src/components/ui/badge";
import type { CalEvent } from "./calendar-api";

const ACCESS_VARIANT: Record<string, BadgeVariant> = {
  owner: "accent",
  editor: "info",
  viewer: "default",
};

export default function EventDetails({
  event,
  onDelete,
  onEdit,
  onShare,
  onSelect,
}: {
  event: CalEvent;
  onDelete(): Promise<void>;
  onEdit?(): void;
  onShare(): void;
  onSelect?(): void;
}) {
  const canEdit = event.access === "owner" || event.access === "editor";

  return (
    <article className="rounded-lg border border-zinc-700 bg-zinc-800/50 p-3">
      <h3 className="font-semibold text-zinc-100">
        {onSelect ? (
          <button
            type="button"
            onClick={onSelect}
            className="text-left hover:underline focus-visible:outline focus-visible:outline-2 focus-visible:outline-accent"
          >
            {event.title}
          </button>
        ) : event.title}
      </h3>
      <p className="mt-1 text-sm text-zinc-400">
        {event.allDay ? "All day" : `${event.startTime.slice(11, 16)} – ${event.endTime.slice(11, 16)}`}
      </p>
      {event.location && <p className="text-sm text-zinc-400">{event.location}</p>}
      {event.description && <p className="mt-2 text-sm text-zinc-300">{event.description}</p>}
      <div className="mt-2"><Badge variant={ACCESS_VARIANT[event.access] ?? "default"}>{event.access}</Badge></div>

      <div className="mt-3 flex flex-wrap gap-2">
        {canEdit && onEdit && (
          <button
            type="button"
            onClick={onEdit}
            className="rounded border border-zinc-700 px-2 py-1 text-sm text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors"
          >
            Edit
          </button>
        )}
        {event.access === "owner" && (
          <button
            type="button"
            onClick={onShare}
            className="rounded border border-zinc-700 px-2 py-1 text-sm text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors"
          >
            Manage sharing
          </button>
        )}
        {canEdit && event.access === "owner" && (
          <button
            type="button"
            onClick={() => void onDelete()}
            className="rounded px-2 py-1 text-sm text-state-danger hover:bg-state-danger/10 transition-colors"
          >
            Delete event
          </button>
        )}
      </div>
    </article>
  );
}
