import { useEffect, useState } from "react";
import {
  clearMaintenance,
  listMaintenance,
  setMaintenance,
  type MaintenanceWindow,
} from "../api";

/** datetime-local value (local time, no zone) → ISO instant, or undefined. */
function localInputToIso(value: string): string | undefined {
  if (!value) return undefined;
  const ms = Date.parse(value);
  return Number.isNaN(ms) ? undefined : new Date(ms).toISOString();
}

function fmt(iso?: string): string {
  if (!iso) return "—";
  const d = new Date(iso);
  return Number.isNaN(d.getTime()) ? iso : d.toLocaleString();
}

/**
 * Operator control for platform maintenance. Lists active windows and sets or
 * clears them. Every call is authorised by Cloud against the operator's session
 * (system:config); this component only presents it.
 */
export default function MaintenanceControls() {
  const [windows, setWindows] = useState<MaintenanceWindow[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const [subjectId, setSubjectId] = useState("");
  const [part, setPart] = useState("");
  const [reason, setReason] = useState("");
  const [expectedEnd, setExpectedEnd] = useState("");

  const refresh = () => {
    listMaintenance()
      .then((w) => setWindows(w))
      .catch(() => setError("Could not load maintenance windows."));
  };

  useEffect(refresh, []);

  async function start(e: React.FormEvent) {
    e.preventDefault();
    if (!subjectId.trim() || !part.trim()) return;
    setBusy(true);
    setError(null);
    try {
      await setMaintenance(subjectId.trim(), {
        part: part.trim(),
        ...(reason.trim() ? { reason: reason.trim() } : {}),
        ...(localInputToIso(expectedEnd) ? { expectedEndAt: localInputToIso(expectedEnd) } : {}),
      });
      setSubjectId("");
      setPart("");
      setReason("");
      setExpectedEnd("");
      refresh();
    } catch {
      setError("Could not set maintenance. Check you are signed in as an operator.");
    } finally {
      setBusy(false);
    }
  }

  async function end(id: string) {
    setBusy(true);
    setError(null);
    try {
      await clearMaintenance(id);
      refresh();
    } catch {
      setError("Could not end maintenance.");
    } finally {
      setBusy(false);
    }
  }

  return (
    <div>
      <h2 className="text-lg font-medium">Maintenance</h2>
      <p className="mt-2 text-sm text-zinc-400">
        Put a platform part into maintenance. Visitors to it see a maintenance page with the
        expected-back time until you end it. The subject is the part's host (e.g.{" "}
        <code>hosting.tnhc.dev</code>) or its tool id (e.g. <code>nexus-chat</code>).
      </p>

      {error && (
        <p role="alert" className="mt-3 text-sm text-state-danger">
          {error}
        </p>
      )}

      {windows === null ? (
        <p className="mt-3 text-sm text-zinc-500">Loading…</p>
      ) : windows.length === 0 ? (
        <p className="mt-3 text-sm text-zinc-400">No parts are in maintenance.</p>
      ) : (
        <ul className="mt-3 divide-y divide-zinc-800 rounded-lg border border-zinc-800">
          {windows.map((w) => (
            <li key={w.subjectId} className="flex items-start justify-between gap-4 p-4">
              <div className="min-w-0">
                <div className="font-medium text-zinc-100">{w.part}</div>
                <div className="text-sm text-zinc-500">{w.subjectId}</div>
                {w.reason && <p className="mt-1 break-words text-sm text-zinc-400">{w.reason}</p>}
                <p className="mt-1 text-xs text-zinc-600">
                  Since {fmt(w.startedAt)} · expected back {fmt(w.expectedEndAt)}
                </p>
              </div>
              <button
                type="button"
                disabled={busy}
                onClick={() => void end(w.subjectId)}
                className="shrink-0 rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800 disabled:opacity-50"
              >
                End maintenance
              </button>
            </li>
          ))}
        </ul>
      )}

      <form onSubmit={start} className="mt-4 grid gap-3 rounded-lg border border-zinc-800 p-4 sm:grid-cols-2">
        <label className="text-sm">
          <span className="text-zinc-400">Subject (host or tool id)</span>
          <input
            value={subjectId}
            onChange={(e) => setSubjectId(e.target.value)}
            placeholder="hosting.tnhc.dev"
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1"
            required
          />
        </label>
        <label className="text-sm">
          <span className="text-zinc-400">Display name</span>
          <input
            value={part}
            onChange={(e) => setPart(e.target.value)}
            placeholder="Nexus Hosting"
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1"
            required
          />
        </label>
        <label className="text-sm sm:col-span-2">
          <span className="text-zinc-400">Reason (optional, shown to visitors)</span>
          <input
            value={reason}
            onChange={(e) => setReason(e.target.value)}
            placeholder="Upgrading the storage layer"
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1"
          />
        </label>
        <label className="text-sm">
          <span className="text-zinc-400">Expected back (optional)</span>
          <input
            type="datetime-local"
            value={expectedEnd}
            onChange={(e) => setExpectedEnd(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-2 py-1"
          />
        </label>
        <div className="flex items-end">
          <button
            type="submit"
            disabled={busy || !subjectId.trim() || !part.trim()}
            className="rounded bg-amber-700 px-4 py-1.5 text-sm font-medium hover:bg-amber-600 disabled:opacity-50"
          >
            Start maintenance
          </button>
        </div>
      </form>
    </div>
  );
}
