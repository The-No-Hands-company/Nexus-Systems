import { useEffect, useState } from "react";
import { ApiError, cloudUsers, type CloudUser } from "../../api";
import CloudNav from "./CloudNav";

type State =
  | { kind: "loading" }
  | { kind: "forbidden" }
  | { kind: "unavailable" }
  | { kind: "ready"; users: CloudUser[] };

/**
 * The ported `view-users` from Cloud's status.html (loadUsersView): every
 * account registered on this node.
 *
 * This is the one Cloud view that lists every user, so it is admin-only —
 * enforced server-side by the dashboard proxy (server.ts CLOUD_ALLOWLIST
 * `users: { adminOnly: true }`), not by hiding this component. A non-admin
 * caller gets a 403 back from the proxy itself; that must render as an
 * explained "you don't have permission" state, distinct from both a generic
 * failure and an empty table — an empty table here would read as "no users
 * exist," which is not what a 403 means.
 */
export default function CloudUsers() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudUsers()
      .then((users) => { if (!cancelled) setState({ kind: "ready", users }); })
      .catch((err: unknown) => {
        if (cancelled) return;
        if (err instanceof ApiError && err.reason === "forbidden") {
          setState({ kind: "forbidden" });
        } else {
          // Covers the proxy's 503 cloud_unavailable, a network failure, and
          // any other upstream error — Cloud must be assumed to be down
          // sometimes, and this page must survive that, not throw or blank.
          setState({ kind: "unavailable" });
        }
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Cloud users</h1>
        <p className="mt-2 text-zinc-400">Every account registered on this node.</p>
      </div>

      {state.kind === "loading" && <p className="text-zinc-500">Loading…</p>}

      {state.kind === "forbidden" && (
        <p role="alert" className="text-amber-400">
          You do not have permission to view this. The users list is restricted to operators.
        </p>
      )}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && state.users.length === 0 && (
        <p className="text-zinc-400">No users registered yet.</p>
      )}

      {state.kind === "ready" && state.users.length > 0 && (
        <div className="overflow-x-auto rounded-lg border border-zinc-800 bg-zinc-900">
          <table className="w-full text-left text-sm">
            <thead>
              <tr className="border-b border-zinc-800 text-xs uppercase tracking-wide text-zinc-500">
                <th className="px-4 py-3 font-medium">Username</th>
                <th className="px-4 py-3 font-medium">NS address</th>
                <th className="px-4 py-3 font-medium">Node ID</th>
                <th className="px-4 py-3 font-medium">Registered</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-zinc-800">
              {state.users.map((u, i) => (
                // eslint-disable-next-line react/no-array-index-key -- users may lack a stable id
                <tr key={u.id ?? `${u.username ?? "user"}-${i}`}>
                  <td className="px-4 py-3 font-medium text-zinc-100">{u.username || "—"}</td>
                  <td className="px-4 py-3 font-mono text-xs text-green-400">{u.address || "—"}</td>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-400">{u.nodeId || "—"}</td>
                  <td className="px-4 py-3 text-zinc-400">
                    {u.registeredAt ? new Date(u.registeredAt).toLocaleString() : "—"}
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
