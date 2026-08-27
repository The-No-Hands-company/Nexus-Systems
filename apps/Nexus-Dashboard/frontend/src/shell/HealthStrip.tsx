import { Link } from "react-router-dom";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import type { AppEntry } from "../api";

const MAX = 8;

/** Offline first: a strip you have to read left-to-right to find the problem is
 *  a strip nobody reads. */
export default function HealthStrip({ apps }: { apps: AppEntry[] }) {
  const healthy = apps.filter((a) => a.health === "healthy").length;
  const ordered = [...apps].sort((a, b) =>
    a.health === b.health ? a.name.localeCompare(b.name) : a.health === "offline" ? -1 : 1,
  );

  return (
    <div className="flex flex-wrap items-center gap-2">
      {ordered.slice(0, MAX).map((a) => (
        <Pill key={a.id} data-testid="health-pill" tone={a.health === "healthy" ? "success" : "danger"}>
          {a.name}
        </Pill>
      ))}
      <Link to="/admin" className="text-xs text-zinc-500 hover:text-zinc-300">
        {healthy} of {apps.length} healthy
      </Link>
    </div>
  );
}
