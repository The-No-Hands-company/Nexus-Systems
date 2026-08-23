import { useEffect, useState } from "react";
import {
  me,
  isAdmin,
  listAccessRequests,
  decideAccessRequest,
  createInvite,
  cloudFederationPeers,
  cloudEndpoints,
  cloudIdentity,
  cloudTools,
  type Me,
  type AccessRequest,
  type CloudPeer,
  type CloudEndpoint,
} from "../api";

/**
 * The operator's surface: who is waiting, and a way to hand someone a code
 * directly. This is what makes invite-only actually operable — without it,
 * approving an account means editing a JSON file by hand.
 *
 * Hiding this from non-admins is presentation only. Every endpoint it calls is
 * guarded by users:approve (or users:create) server-side, which is what
 * actually enforces the boundary; a hidden panel is a courtesy, not a control.
 */
export default function Admin() {
  const [user, setUser] = useState<Me | null | undefined>(undefined);
  const [requests, setRequests] = useState<AccessRequest[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [invite, setInvite] = useState<{ code: string; expiresAt: string } | null>(null);
  // Dev-tools state. Declared unconditionally: hooks must run in the same
  // order every render, so none of these may sit below the early returns.
  const [devNotes, setDevNotes] = useState("");
  const [showDevTools, setShowDevTools] = useState(false);
  const [endpoints, setEndpoints] = useState<CloudEndpoint[]>([]);
  const [peers, setPeers] = useState<CloudPeer[]>([]);
  const [identity, setIdentity] = useState<{ address?: string; shortId?: string } | null>(null);
  const [tools, setTools] = useState<{ id: string; name: string; health?: string; publicUrl?: string }[]>([]);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const u = await me();
      if (cancelled) return;
      setUser(u);
      if (!isAdmin(u)) return;
      try {
        const list = await listAccessRequests();
        if (!cancelled) setRequests(list);
      } catch {
        if (!cancelled) setError("Could not load the request queue.");
      }
    })();
    return () => { cancelled = true; };
  }, []);

  // Fetch cloud data only when dev tools are shown — but the effect itself is
  // unconditional, which is what keeps hook order stable across renders.
  useEffect(() => {
    if (!showDevTools) return;
    let cancelled = false;
    void (async () => {
      try {
        const [eps, p, id, t] = await Promise.all([
          cloudEndpoints().catch(() => []),
          cloudFederationPeers().catch(() => []),
          cloudIdentity().catch(() => null),
          cloudTools().catch(() => []),
        ]);
        if (!cancelled) {
          setEndpoints(eps);
          setPeers(p);
          setIdentity(id);
          setTools(t);
        }
      } catch {}
    })();
    return () => { cancelled = true; };
  }, [showDevTools]);

  async function decide(id: string, decision: "approve" | "reject") {
    setError(null);
    try {
      await decideAccessRequest(id, decision);
      setRequests((list) => list.filter((r) => r.id !== id));
    } catch {
      setError(`Could not ${decision} that request. It may already have been decided.`);
    }
  }

  async function mint() {
    setError(null);
    try {
      setInvite(await createInvite());
    } catch {
      setError("Could not create an invite.");
    }
  }

  if (user === undefined) {
    return <section className="mx-auto max-w-3xl p-8 text-zinc-500">Loading…</section>;
  }

  if (!isAdmin(user)) return null;

  return (
    <section className="mx-auto max-w-3xl space-y-10 p-8">
      <div>
        <h1 className="text-2xl font-semibold">Operator</h1>
        <p className="mt-2 text-zinc-400">Approve access requests and hand out invites.</p>
      </div>

      {error && <p role="alert" className="text-sm text-red-400">{error}</p>}

      <div>
        <h2 className="text-lg font-medium">Pending requests</h2>
        {requests.length === 0 ? (
          <p className="mt-2 text-sm text-zinc-400">No pending requests.</p>
        ) : (
          <ul className="mt-3 divide-y divide-zinc-800 rounded-lg border border-zinc-800">
            {requests.map((r) => (
              <li key={r.id} className="flex items-start justify-between gap-4 p-4">
                <div className="min-w-0">
                  <div className="font-medium text-zinc-100">{r.username}</div>
                  <div className="text-sm text-zinc-500">{r.email}</div>
                  {r.note && (
                    <p className="mt-1 break-words text-sm text-zinc-400">{r.note}</p>
                  )}
                </div>
                <div className="flex shrink-0 gap-2">
                  <button
                    type="button" onClick={() => void decide(r.id, "approve")}
                    className="rounded bg-green-700 px-3 py-1 text-sm"
                  >
                    Approve
                  </button>
                  <button
                    type="button" onClick={() => void decide(r.id, "reject")}
                    className="rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800"
                  >
                    Reject
                  </button>
                </div>
              </li>
            ))}
          </ul>
        )}
        <p className="mt-2 text-xs text-zinc-600">
          Approving does not notify anyone — there is no outbound email. The person returns to
          /claim with the code they were given when they asked.
        </p>
      </div>

      <div>
        <h2 className="text-lg font-medium">Invites</h2>
        <p className="mt-2 text-sm text-zinc-400">
          An invite skips the queue: whoever redeems it sets a password immediately.
        </p>

        {invite ? (
          <div className="mt-3 rounded-lg border border-amber-500/40 bg-amber-500/5 p-4">
            <div className="text-xs uppercase tracking-wide text-amber-400">Invite code</div>
            <code className="mt-2 block break-all font-mono text-lg text-amber-200">{invite.code}</code>
            <p className="mt-2 text-sm text-amber-200/80">
              This is the only time the code is shown. Save it before you leave this page.
            </p>
            <button
              type="button" onClick={() => setInvite(null)}
              className="mt-3 rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800"
            >
              Done
            </button>
          </div>
        ) : (
          <button
            type="button" onClick={() => void mint()}
            className="mt-3 rounded bg-blue-600 px-4 py-2 font-medium"
          >
            Create invite
          </button>
        )}
      </div>

      {/* Development Notes & Tools (Founder only, development phase) */}
      <div className="rounded-lg border border-zinc-800 bg-zinc-900/30 p-6 space-y-6">
        <div className="flex items-center justify-between">
          <h2 className="text-lg font-medium">Development Notes & Tools</h2>
          <button
            type="button"
            onClick={() => setShowDevTools(!showDevTools)}
            className="rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800"
          >
            {showDevTools ? "Hide" : "Show"} Tools
          </button>
        </div>

        <div>
          <label htmlFor="dev-notes" className="block text-sm text-zinc-400 mb-2">
            Development Notes (Founder only, persisted locally)
          </label>
          <textarea
            id="dev-notes"
            value={devNotes}
            onChange={(e) => setDevNotes(e.target.value)}
            className="w-full min-h-[120px] rounded bg-zinc-800 border border-zinc-700 p-3 text-sm font-mono text-zinc-100 placeholder-zinc-500 focus:border-blue-500 focus:outline-none focus:ring-1 focus:ring-blue-500"
            placeholder="Add development notes here... e.g. remember to create dev helper tool for listing all exposed domains..."
          />
        </div>

        {showDevTools && (
          <div className="space-y-6 border-t border-zinc-800 pt-6">
            <div>
              <h3 className="font-medium mb-3">Cloud Identity</h3>
              <div className="grid gap-2 text-sm font-mono text-zinc-300">
                <div><span className="text-zinc-500">Address: </span>{identity?.address ?? "—"}</div>
                <div><span className="text-zinc-500">Short ID: </span>{identity?.shortId ?? "—"}</div>
              </div>
            </div>

            <div>
              <h3 className="font-medium mb-3">Cloud Tools ({tools.length})</h3>
              <div className="rounded-lg border border-zinc-800 bg-zinc-900/30 p-3 max-h-60 overflow-auto">
                <table className="w-full text-sm font-mono">
                  <thead>
                    <tr className="text-zinc-500 border-b border-zinc-800">
                      <th className="text-left pb-2">ID</th>
                      <th className="text-left pb-2">Name</th>
                      <th className="text-left pb-2">Health</th>
                      <th className="text-left pb-2">Public URL</th>
                    </tr>
                  </thead>
                  <tbody>
                    {tools.map((t) => (
                      <tr key={t.id} className="border-b border-zinc-800/50">
                        <td className="pb-2 text-zinc-400">{t.id}</td>
                        <td className="pb-2 text-zinc-200">{t.name}</td>
                        <td className="pb-2">{t.health ?? "—"}</td>
                        <td className="pb-2 text-zinc-500">{t.publicUrl ?? "—"}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>

            <div>
              <h3 className="font-medium mb-3">Cloud Endpoints ({endpoints.length})</h3>
              <div className="rounded-lg border border-zinc-800 bg-zinc-900/30 p-3 max-h-60 overflow-auto">
                <table className="w-full text-sm font-mono">
                  <thead>
                    <tr className="text-zinc-500 border-b border-zinc-800">
                      <th className="text-left pb-2">Method</th>
                      <th className="text-left pb-2">Path</th>
                      <th className="text-left pb-2">Description</th>
                    </tr>
                  </thead>
                  <tbody>
                    {endpoints.map((e, i) => (
                      <tr key={i} className="border-b border-zinc-800/50">
                        <td className="pb-2 text-blue-400">{e.method}</td>
                        <td className="pb-2 text-zinc-300">{e.path}</td>
                        <td className="pb-2 text-zinc-500">{e.description ?? "—"}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>

            <div>
              <h3 className="font-medium mb-3">Federation Peers ({peers.length})</h3>
              <div className="rounded-lg border border-zinc-800 bg-zinc-900/30 p-3 max-h-60 overflow-auto">
                <table className="w-full text-sm font-mono">
                  <thead>
                    <tr className="text-zinc-500 border-b border-zinc-800">
                      <th className="text-left pb-2">Domain</th>
                      <th className="text-left pb-2">Trust</th>
                      <th className="text-left pb-2">Status</th>
                    </tr>
                  </thead>
                  <tbody>
                    {peers.map((p, i) => (
                      <tr key={i} className="border-b border-zinc-800/50">
                        <td className="pb-2 text-zinc-300">{p.domain ?? "—"}</td>
                        <td className="pb-2">{String(p.trust ?? p.trustLevel ?? "—")}</td>
                        <td className="pb-2 text-zinc-500">{p.trust ? "object" : "string"}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        )}
      </div>
    </section>
  );
}