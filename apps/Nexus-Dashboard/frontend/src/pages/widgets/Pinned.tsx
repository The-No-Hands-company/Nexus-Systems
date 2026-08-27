import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { listApps, type AppEntry } from "../../api";
import WidgetShell, { type WidgetState } from "./WidgetShell";

export default function Pinned() {
  const [state, setState] = useState<WidgetState<AppEntry[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    listApps()
      .then((apps) => {
        if (!cancelled) setState({ status: "ready", data: apps.filter((a) => a.health === "healthy") });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Apps unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Pinned" state={state} empty="No apps pinned yet">
      {(apps) =>
        apps.length === 0 ? null : (
          <ul className="flex flex-col gap-1">
            {apps.map((a) => (
              <li key={a.id} className="border-b border-zinc-700 py-1 text-sm last:border-0">
                <Link to={a.path} className="text-zinc-200 hover:text-accent">{a.name}</Link>
              </li>
            ))}
          </ul>
        )
      }
    </WidgetShell>
  );
}
