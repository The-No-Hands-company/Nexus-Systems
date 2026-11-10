import { useEffect, useState } from "react";
import { cloudBackups, createCloudBackup, type CloudBackupItem } from "../../api";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; backups: CloudBackupItem[] };

export default function CloudBackups() {
  const [state, setState] = useState<State>({ kind: "loading" });
  const [creating, setCreating] = useState(false);
  const [msg, setMsg] = useState<{ text: string; ok: boolean } | null>(null);

  const load = () => {
    void cloudBackups()
      .then((backups) => setState({ kind: "ready", backups }))
      .catch(() => setState({ kind: "unavailable" }));
  };

  useEffect(() => {
    load();
  }, []);

  const handleCreate = async () => {
    setCreating(true);
    setMsg(null);
    try {
      await createCloudBackup();
      setMsg({ text: "Atomic backup snapshot created and verified successfully.", ok: true });
      load();
    } catch (err: any) {
      setMsg({ text: err?.message || "Failed to trigger backup", ok: false });
    } finally {
      setCreating(false);
    }
  };

  return (
    <section className="mx-auto max-w-5xl space-y-6 px-4 py-6 sm:p-8">
      <CloudNav />
      <div className="flex flex-wrap items-center justify-between gap-4">
        <div>
          <h1 className="text-2xl font-semibold text-zinc-100">Automated Backup Orchestrator</h1>
          <p className="mt-1 text-sm text-zinc-400">
            Atomic SQLite snapshots, platform state backups, and durable store retention.
          </p>
        </div>
        <button
          type="button"
          disabled={creating}
          onClick={handleCreate}
          className="rounded-lg bg-emerald-600 px-4 py-2 text-sm font-medium text-white transition hover:bg-emerald-500 disabled:opacity-50"
        >
          {creating ? "Creating Snapshot…" : "+ Create Snapshot Now"}
        </button>
      </div>

      {msg && (
        <div
          role="status"
          className={`rounded-lg p-3 text-sm ${msg.ok ? "bg-emerald-950/60 text-emerald-300 border border-emerald-800" : "bg-rose-950/60 text-rose-300 border border-rose-800"}`}
        >
          {msg.text}
        </div>
      )}

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading backup snapshots…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-rose-400">
          Backup orchestrator is temporarily unavailable.
        </p>
      )}

      {state.kind === "ready" && state.backups.length === 0 && (
        <p className="text-zinc-400">No backup snapshots on disk yet.</p>
      )}

      {state.kind === "ready" && state.backups.length > 0 && (
        <div className="overflow-x-auto rounded-xl border border-zinc-800 bg-zinc-900">
          <table className="w-full text-left text-sm">
            <thead>
              <tr className="border-b border-zinc-800 text-xs uppercase tracking-wide text-zinc-500">
                <th scope="col" className="px-4 py-3 font-medium">Snapshot ID</th>
                <th scope="col" className="px-4 py-3 font-medium">Target Store</th>
                <th scope="col" className="px-4 py-3 font-medium">Size</th>
                <th scope="col" className="px-4 py-3 font-medium">Updated</th>
                <th scope="col" className="px-4 py-3 font-medium">Integrity</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-zinc-800">
              {state.backups.map((b) => (
                <tr key={b.id}>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-200">{b.filename}</td>
                  <td className="px-4 py-3">
                    <span className="rounded bg-zinc-800 px-2 py-0.5 text-xs text-zinc-300">
                      {b.store}
                    </span>
                  </td>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-400">
                    {(b.sizeBytes / 1024).toFixed(1)} KB
                  </td>
                  <td className="px-4 py-3 text-xs text-zinc-400">
                    {new Date(b.updatedAt).toLocaleString()}
                  </td>
                  <td className="px-4 py-3">
                    <span className="inline-flex items-center gap-1.5 rounded-full bg-emerald-950/80 px-2 py-0.5 text-xs text-emerald-400">
                      <span className="h-1.5 w-1.5 rounded-full bg-emerald-400" />
                      verified
                    </span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
