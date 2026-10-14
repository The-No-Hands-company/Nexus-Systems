import type { Status, StatusCategory } from "../types";

const CATEGORY_COLORS: Record<StatusCategory, string> = {
  backlog: "bg-zinc-700 text-zinc-300",
  unstarted: "bg-zinc-600 text-zinc-200",
  started: "bg-blue-700 text-blue-100",
  completed: "bg-green-700 text-green-100",
  canceled: "bg-zinc-800 text-zinc-500",
};

export function StatusBadge({ status }: { status: Status }) {
  return (
    <span className={`inline-flex items-center px-2 py-0.5 rounded text-xs font-medium ${CATEGORY_COLORS[status.category]}`}>
      {status.name}
    </span>
  );
}
