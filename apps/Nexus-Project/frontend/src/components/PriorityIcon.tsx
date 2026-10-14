import type { Priority } from "../types";

const CONFIG: Record<Priority, { color: string; label: string; symbol: string }> = {
  none: { color: "text-zinc-500", label: "No priority", symbol: "–" },
  low: { color: "text-blue-400", label: "Low", symbol: "↓" },
  medium: { color: "text-yellow-400", label: "Medium", symbol: "→" },
  high: { color: "text-orange-400", label: "High", symbol: "↑" },
  urgent: { color: "text-red-400", label: "Urgent", symbol: "!!" },
};

export function PriorityIcon({ priority }: { priority: Priority }) {
  const { color, label, symbol } = CONFIG[priority];
  return (
    <span className={`inline-block w-5 text-center font-bold text-sm ${color}`} title={label}>
      {symbol}
    </span>
  );
}
