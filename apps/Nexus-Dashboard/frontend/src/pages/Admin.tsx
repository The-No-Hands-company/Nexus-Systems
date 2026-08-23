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
  listDevTools,
  runDevTool,
  type Me,
  type AccessRequest,
  type CloudPeer,
  type CloudEndpoint,
  type DevTool,
  type DevToolsResponse,
  type CloudTool,
  listServiceHealth,
  listUsers,
  updateUserRole,
  suspendUser,
  activateUser,
  type ServiceStatus,
  type ManagedUser,
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
  // Open by default: this whole panel exists for exactly these tools, and
  // hiding them behind a toggle was how they went "missing" in practice.
  const [showDevTools, setShowDevTools] = useState(true);
  const [endpoints, setEndpoints] = useState<CloudEndpoint[]>([]);
  const [peers, setPeers] = useState<CloudPeer[]>([]);
  const [identity, setIdentity] = useState<{ address?: string; shortId?: string } | null>(null);
  const [tools, setTools] = useState<CloudTool[]>([]);
  const [dhts, setDhts] = useState<DevToolsResponse | null>(null);
  const [runningId, setRunningId] = useState<string | null>(null);
  const [toolOutput, setToolOutput] = useState<Record<string, { ok: boolean; output: string }>>({});
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [users, setUsers] = useState<ManagedUser[]>([]);
  const [userError, setUserError] = useState<string | null>(null);

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
        const [eps, p, id, t, d, sv, us] = await Promise.all([
          cloudEndpoints().catch(() => []),
          cloudFederationPeers().catch(() => []),
          cloudIdentity().catch(() => null),
          cloudTools().catch(() => []),
          listDevTools().catch(() => null),
          listServiceHealth().catch(() => ({ services: [] })),
          isAdmin(user ?? null) ? listUsers().catch(() => []) : Promise.resolve([]),
        ]);
        if (!cancelled) {
          setEndpoints(eps);
          setPeers(p);
          setIdentity(id);
          setTools(t);
          setDhts(d);
          setServices(sv.services);
          if (Array.isArray(us)) setUsers(us);
        }
      } catch {}
    })();
    return () => { cancelled = true; };
  }, [showDevTools, user]);

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
              <h3 className="font-medium mb-1">Development Helper Tools (dhts/)</h3>
              <p className="text-xs text-zinc-500 mb-3">
                {dhts?.exists
                  ? `${dhts.tools.length} tools from ${"dhts/"} — runnable entries execute on this host, founder-only.`
                  : "dhts/ not found on the server."}
              </p>
              <div className="grid gap-3 sm:grid-cols-2">
                {(dhts?.tools ?? []).map((tool: DevTool) => (
                  <div key={tool.id} className="rounded-lg border border-zinc-800 bg-zinc-900/40 p-4 flex flex-col gap-2">
                    <div className="flex items-start justify-between gap-2">
                      <span className="font-medium text-sm text-zinc-100">{tool.name}</span>
                      {tool.runnable && (
                        <button
                          type="button"
                          disabled={runningId !== null}
                          onClick={async () => {
                            setRunningId(tool.id);
                            try {
                              const r = await runDevTool(tool.id);
                              setToolOutput((prev) => ({ ...prev, [tool.id]: { ok: r.ok, output: r.output || r.error || `exit ${r.exitCode}` } }));
                            } catch {
                              setToolOutput((prev) => ({ ...prev, [tool.id]: { ok: false, output: "request failed" } }));
                            } finally {
                              setRunningId(null);
                            }
                          }}
                          className="rounded bg-blue-600 px-2 py-0.5 text-xs font-medium disabled:opacity-50 shrink-0"
                        >
                          {runningId === tool.id ? "Running…" : "Run"}
                        </button>
                      )}
                    </div>
                    <p className="text-xs leading-relaxed text-zinc-400">{tool.description}</p>
                    <code className="block break-all rounded bg-black/40 px-2 py-1 text-[11px] text-zinc-500">{tool.command}</code>
                    {toolOutput[tool.id] && (
                      <pre
                        className={`mt-1 max-h-48 overflow-auto rounded bg-black/60 p-2 text-[11px] leading-snug ${
                          toolOutput[tool.id]!.ok ? "text-emerald-300" : "text-red-300"
                        }`}
                      >
                        {toolOutput[tool.id]!.output}
                      </pre>
                    )}
                  </div>
                ))}
                {dhts?.exists && dhts.tools.length === 0 && (
                  <p className="text-sm text-zinc-500">No tools registered and none discovered.</p>
                )}
              </div>
            </div>

            <div>
              <h3 className="font-medium mb-3">Service Health</h3>
              <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-4">
                {services.map((svc) => (
                  <div key={svc.name} className="rounded-lg border border-zinc-800 bg-zinc-900/40 p-3 flex items-center gap-2">
                    <span aria-hidden="true" className={`h-2 w-2 rounded-full shrink-0 ${svc.healthy ? "bg-emerald-400" : "bg-red-400"}`} />
                    <div className="min-w-0">
                      <div className="text-sm font-medium text-zinc-200 truncate">{svc.name}</div>
                      <div className="text-xs text-zinc-500 truncate">
                        {svc.healthy ? `${svc.latencyMs ?? "?"}ms` : svc.detail ?? "down"}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>

            <div>
              <h3 className="font-medium mb-3">Users ({users.length})</h3>
              {userError && <p role="alert" className="text-sm text-red-400 mb-2">{userError}</p>}
              <div className="rounded-lg border border-zinc-800 bg-zinc-900/30 p-3 max-h-72 overflow-auto">
                <table className="w-full text-sm font-mono">
                  <thead>
                    <tr className="text-zinc-500 border-b border-zinc-800">
                      <th className="text-left pb-2">Username</th>
                      <th className="text-left pb-2">Role</th>
                      <th className="text-left pb-2">Status</th>
                      <th className="text-right pb-2">Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    {users.map((u) => (
                      <tr key={u.id} className="border-b border-zinc-800/50">
                        <td className="pb-2 pt-2">
                          <span className="text-zinc-200">{u.username}</span>
                          <span className="block text-xs text-zinc-500">{u.email}</span>
                        </td>
                        <td className="pb-2 pt-2">
                          <select
                            value={u.role}
                            onChange={async (e) => {
                              const role = e.target.value;
                              try {
                                await updateUserRole(u.id, role);
                                setUsers((list) => list.map((x) => x.id === u.id ? { ...x, role } : x));
                              } catch { setUserError("Could not update role."); }
                            }}
                            disabled={u.id === user?.id}
                            className="rounded bg-zinc-800 border border-zinc-700 text-xs text-zinc-200 px-1 py-0.5"
                          >
                            {["user", "admin", "founder"].map((r) => (
                              <option key={r} value={r}>{r}</option>
                            ))}
                          </select>
                        </td>
                        <td className="pb-2 pt-2">
                          <span className={u.status === "active" ? "text-emerald-400" : "text-red-400"}>
                            {u.status}
                          </span>
                        </td>
                        <td className="pb-2 pt-2 text-right">
                          {u.id !== user?.id && (
                            <button
                              type="button"
                              onClick={async () => {
                                try {
                                  if (u.status === "active") await suspendUser(u.id);
                                  else await activateUser(u.id);
                                  setUsers((list) => list.map((x) => x.id === u.id ? { ...x, status: u.status === "active" ? "suspended" : "active" } : x));
                                } catch { setUserError("Could not update status."); }
                              }}
                              className={`rounded px-2 py-0.5 text-xs ${u.status === "active" ? "bg-red-900/50 text-red-300 hover:bg-red-900" : "bg-green-900/50 text-green-300 hover:bg-green-900"}`}
                            >
                              {u.status === "active" ? "Suspend" : "Activate"}
                            </button>
                          )}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <p className="mt-1 text-xs text-zinc-600">Role changes revoke existing sessions automatically.</p>
            </div>

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
                      <tr key={t.id ?? t.name} className="border-b border-zinc-800/50">
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