import { useEffect, useState } from "react";
import { cloudFederationPeers, type CloudPeer } from "../../api";
import { timeAgo } from "./format";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; peers: CloudPeer[] };


/**
 * The ported `view-federation` from Cloud's status.html (loadFederationView):
 * every peer this node has discovered.
 */
export default function CloudFederation() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudFederationPeers()
      .then((peers) => { if (!cancelled) setState({ kind: "ready", peers }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 px-4 py-6 sm:p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Federation</h1>
        <p className="mt-2 text-zinc-400">Peers this node has discovered.</p>
      </div>

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && state.peers.length === 0 && (
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-6 text-center">
          <p className="text-zinc-300">No federation peers discovered yet.</p>
          <p className="mt-2 text-sm text-zinc-500">
            Add BOOTSTRAP_PEERS to your .env to connect to other Nexus nodes.
          </p>
        </div>
      )}

      {state.kind === "ready" && state.peers.length > 0 && (
        <div className="overflow-x-auto rounded-lg border border-zinc-800 bg-zinc-900">
          <table className="w-full text-left text-sm">
            <thead>
              <tr className="border-b border-zinc-800 text-xs uppercase tracking-wide text-zinc-500">
                <th scope="col" className="px-4 py-3 font-medium">Domain</th>
                <th scope="col" className="px-4 py-3 font-medium">Trust</th>
                <th scope="col" className="px-4 py-3 font-medium">Last seen</th>
                <th scope="col" className="px-4 py-3 font-medium">DID</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-zinc-800">
              {state.peers.map((p, i) => (
                // eslint-disable-next-line react/no-array-index-key -- peers may lack a stable id
                <tr key={p.domain ?? i}>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-100">{p.domain}</td>
                  <td className="px-4 py-3 text-zinc-300">{p.trustState ?? "—"}</td>
                  <td className="px-4 py-3 text-zinc-400">{p.lastSeenAt ? timeAgo(p.lastSeenAt) : "—"}</td>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-400">
                    {p.did ?? "—"}
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
