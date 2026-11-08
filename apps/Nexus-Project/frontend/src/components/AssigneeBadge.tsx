interface Props {
  subject: string | null;
  size?: "sm" | "md";
  showName?: boolean;
  className?: string;
}

function stringToColor(str: string): string {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash = str.charCodeAt(i) + ((hash << 5) - hash);
  }
  const colors = [
    "bg-indigo-600 text-indigo-100",
    "bg-emerald-600 text-emerald-100",
    "bg-amber-600 text-amber-100",
    "bg-rose-600 text-rose-100",
    "bg-sky-600 text-sky-100",
    "bg-violet-600 text-violet-100",
    "bg-teal-600 text-teal-100",
    "bg-fuchsia-600 text-fuchsia-100",
  ];
  return colors[Math.abs(hash) % colors.length];
}

function formatSubject(sub: string): { label: string; initial: string } {
  if (sub.startsWith("did:key:")) {
    const raw = sub.replace("did:key:", "");
    return { label: `${raw.slice(0, 4)}…${raw.slice(-3)}`, initial: raw.slice(0, 2).toUpperCase() };
  }
  if (sub.includes("@")) {
    const name = sub.split("@")[0];
    return { label: name, initial: name.slice(0, 2).toUpperCase() };
  }
  return { label: sub, initial: sub.slice(0, 2).toUpperCase() };
}

export function AssigneeBadge({ subject, size = "sm", showName = false, className = "" }: Props) {
  if (!subject) return null;

  const { label, initial } = formatSubject(subject);
  const colorClass = stringToColor(subject);
  const sizeClass = size === "sm" ? "w-5 h-5 text-[10px]" : "w-6 h-6 text-xs";

  return (
    <div
      className={`inline-flex items-center gap-1.5 select-none ${className}`}
      title={`Assignee: ${subject}`}
    >
      <span
        className={`inline-flex items-center justify-center font-bold rounded-full ${sizeClass} ${colorClass}`}
        aria-hidden="true"
      >
        {initial}
      </span>
      {showName && <span className="text-xs text-zinc-300 truncate max-w-[120px]">{label}</span>}
    </div>
  );
}
