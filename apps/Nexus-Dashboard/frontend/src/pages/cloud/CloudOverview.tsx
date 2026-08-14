import { useEffect, useState } from "react";
import {
  cloudStatus, cloudTrust, cloudIdentity, cloudAudit, cloudTools,
  type CloudStatus, type CloudTrust, type CloudIdentity, type CloudAuditEvent, type CloudTool,
} from "../../api";
import { timeAgo, healthDotClass, healthTextClass } from "./format";
import CloudNav from "./CloudNav";

/** status.html's hardcoded INTERNAL array in renderInternalServicesCard. */
const INTERNAL_SERVICES = [
  {
    id: "nexus-guardian",
    icon: "🛡️",
    name: "Nexus Guardian",
    desc: "Behavior guardrails & policy enforcement. Approves or denies service exposure and records trust decisions.",
  },
  {
    id: "nexus-edge",
    icon: "🔍",
    name: "Nexus Edge",
    desc: "Threat detection & anomaly monitoring. Records threats, assigns severity, recommends responses.",
  },
  {
    id: "nexus-tunnel",
    icon: "🌏",
    name: "Nexus Tunnel",
    desc: "Exposure management & routing. Controls which services get public URLs and gates access via Guardian.",
  },
] as const;

type Ready = {
  kind: "ready";
  status: CloudStatus;
  identity: CloudIdentity;
  trust: CloudTrust | null;
  audit: CloudAuditEvent[];
  tools: CloudTool[];
};

type State = { kind: "loading" } | { kind: "unavailable" } | Ready;

const TRUST_STATES = ["trusted", "verified", "pending", "quarantined", "revoked", "expired"] as const;

function TrustPills({ counts }: { counts: import("../../api").CloudTrustCounts }) {
  const pills = TRUST_STATES.map((key) => ({ key, n: counts[key] ?? 0 })).filter((p) => p.n > 0);
  if (pills.length === 0) {
    return <span className="rounded-full bg-zinc-800 px-2 py-0.5 text-xs text-zinc-500">none</span>;
  }
  return (
    <>
      {pills.map((p) => (
        <span
          key={p.key}
          className="mr-1.5 inline-block rounded-full bg-zinc-800 px-2 py-0.5 text-xs text-zinc-300"
        >
          {p.n} {p.key}
        </span>
      ))}
    </>
  );
}

/**
 * The ported `view-dashboard` from Cloud's status.html: node identity, trust
 * lifecycle, recent trust actions, and the three internal-service tiles
 * (Guardian, Edge, Tunnel). See loadDashboard / renderTrustCard /
 * renderAuditCard / renderInternalServicesCard there for the source of truth.
 *
 * `view-launcher` and `view-app-frame` from the same file are not ported —
 * this shell already is that, and duplicating it would be exactly the thing
 * this migration removes. See the design doc for that call.
 */
export default function CloudOverview() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      try {
        // status and identity are load-bearing — status.html throws the whole
        // dashboard into its error state if either fails, so we do too.
        const [status, identity] = await Promise.all([cloudStatus(), cloudIdentity()]);
        if (cancelled) return;
        // Trust, audit and the tool registry are each optional in the
        // original: a failure there omits that card rather than the page.
        const [trust, audit, tools] = await Promise.all([
          cloudTrust().catch(() => null),
          cloudAudit().catch(() => [] as CloudAuditEvent[]),
          cloudTools().catch(() => [] as CloudTool[]),
        ]);
        if (cancelled) return;
        setState({ kind: "ready", status, identity, trust, audit, tools });
      } catch {
        if (!cancelled) setState({ kind: "unavailable" });
      }
    })();
    return () => { cancelled = true; };
  }, []);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-5xl p-8 text-zinc-500">Loading…</section>;
  }

  if (state.kind === "unavailable") {
    return (
      <section className="mx-auto max-w-5xl space-y-6 p-8">
        <CloudNav />
        <h1 className="text-2xl font-semibold">Cloud</h1>
        <p role="alert" className="mt-4 text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      </section>
    );
  }

  const { status, identity, trust, audit, tools } = state;
  const toolsState = status.tools ?? {};
  const usersState = status.users ?? {};
  const peersState = status.peers ?? {};
  const node = status.node ?? {};
  const byId = new Map(tools.map((t) => [t.id, t]));

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Cloud</h1>
        <p className="mt-2 text-zinc-400">This node's control plane — identity, trust and services.</p>
      </div>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">
            {toolsState.healthy ?? toolsState.total ?? 0}
          </div>
          <div className="text-sm text-zinc-400">Healthy tools</div>
          <div className="mt-1 text-xs text-zinc-500">{toolsState.total ?? 0} registered</div>
        </div>
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">{usersState.total ?? 0}</div>
          <div className="text-sm text-zinc-400">Users</div>
          <div className="mt-1 text-xs text-zinc-500">on this node</div>
        </div>
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">{peersState.total ?? peersState.known ?? 0}</div>
          <div className="text-sm text-zinc-400">Peers</div>
          <div className="mt-1 text-xs text-zinc-500">federation nodes</div>
        </div>
      </div>

      <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
        <div className="font-medium text-zinc-100">{identity.address || node.id || "Nexus Node"}</div>
        <div className="text-sm text-zinc-500">{identity.shortId || node.shortId || ""}</div>
        <dl className="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-3">
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">DID</dt>
            <dd className="mt-1 break-all font-mono text-sm text-zinc-300">
              {identity.did || node.did || "—"}
            </dd>
          </div>
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">Short ID</dt>
            <dd className="mt-1 font-mono text-sm text-zinc-300">{identity.shortId || node.shortId || "—"}</dd>
          </div>
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">NS Address</dt>
            <dd className="mt-1 break-all font-mono text-sm text-zinc-300">{identity.address || "—"}</dd>
          </div>
        </dl>
      </div>

      {trust && (
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="flex items-center justify-between">
            <h2 className="text-lg font-medium">Trust lifecycle</h2>
            <span className="text-xs text-zinc-500">updated {timeAgo(trust.updatedAt)}</span>
          </div>
          <div className="mt-3 grid grid-cols-1 gap-4 sm:grid-cols-2">
            <div>
              <div className="text-xs uppercase tracking-wide text-zinc-500">
                Nodes ({trust.nodes.total ?? 0} total)
              </div>
              <div className="mt-2"><TrustPills counts={trust.nodes} /></div>
            </div>
            <div>
              <div className="text-xs uppercase tracking-wide text-zinc-500">
                Peers ({trust.peers.total ?? 0} total)
              </div>
              <div className="mt-2"><TrustPills counts={trust.peers} /></div>
            </div>
          </div>
        </div>
      )}

      {audit.length > 0 && (
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="flex items-center justify-between">
            <h2 className="text-lg font-medium">Recent trust actions</h2>
            <span className="text-xs text-zinc-500">{audit.length}</span>
          </div>
          <ul className="mt-3 divide-y divide-zinc-800">
            {audit.map((ev, i) => {
              const md = ev.metadata ?? {};
              const transition = md.previousState && md.nextState
                ? `${md.previousState} → ${md.nextState}`
                : md.nextState ?? "";
              return (
                // eslint-disable-next-line react/no-array-index-key -- events carry no stable id
                <li key={`${ev.subjectId ?? "event"}-${i}`} className="flex items-start justify-between gap-4 py-3">
                  <div className="min-w-0">
                    <div className="flex items-center gap-2">
                      <span className="rounded bg-zinc-800 px-2 py-0.5 text-xs uppercase text-zinc-300">
                        {md.action || "unknown"}
                      </span>
                      <span className="truncate font-mono text-sm text-zinc-200">{ev.subjectId || "—"}</span>
                    </div>
                    <div className="mt-1 text-xs text-zinc-500">
                      {transition && <span className="mr-2">{transition}</span>}
                      by {md.actor || "system"}
                    </div>
                    {md.reason && <div className="mt-1 text-xs text-zinc-400">{md.reason}</div>}
                  </div>
                  <div className="shrink-0 text-xs text-zinc-500">{timeAgo(ev.timestamp)}</div>
                </li>
              );
            })}
          </ul>
        </div>
      )}

      <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
        <div className="flex items-center justify-between">
          <h2 className="text-lg font-medium">Internal cloud services</h2>
          <span className="text-xs text-zinc-500">
            {INTERNAL_SERVICES.filter((s) => byId.has(s.id)).length}/{INTERNAL_SERVICES.length} online
          </span>
        </div>
        <ul className="mt-3 divide-y divide-zinc-800">
          {INTERNAL_SERVICES.map((svc) => {
            const t = byId.get(svc.id);
            return (
              <li key={svc.id} className="flex items-start gap-3 py-3">
                <div className="text-xl leading-none">{svc.icon}</div>
                <div className="min-w-0 flex-1">
                  <div className="flex flex-wrap items-center gap-2">
                    <span className="font-medium text-zinc-100">{svc.name}</span>
                    {t ? (
                      <span className={`flex items-center gap-1 text-xs ${healthTextClass(t.health)}`}>
                        <span className={`h-1.5 w-1.5 rounded-full ${healthDotClass(t.health)}`} />
                        {t.health || "unknown"}
                      </span>
                    ) : (
                      <span className="rounded bg-amber-500/10 px-1.5 py-0.5 text-xs uppercase text-amber-400">
                        not registered
                      </span>
                    )}
                  </div>
                  <div className="mt-1 text-sm text-zinc-400">{svc.desc}</div>
                  {t && (
                    <div className="mt-1.5 flex flex-wrap items-center gap-1.5">
                      {(t.capabilities ?? []).slice(0, 4).map((c) => (
                        <span key={c} className="rounded bg-zinc-800 px-1.5 py-0.5 text-xs text-zinc-400">
                          {c}
                        </span>
                      ))}
                      <span className="text-xs text-zinc-500">
                        {t.lastHeartbeatAt
                          ? timeAgo(t.lastHeartbeatAt)
                          : t.registeredAt
                            ? `registered ${timeAgo(t.registeredAt)}`
                            : "—"}
                      </span>
                    </div>
                  )}
                </div>
                {t && (t.publicUrl || t.upstreamUrl) && (
                  <a
                    href={t.publicUrl || t.upstreamUrl}
                    target="_blank"
                    rel="noreferrer"
                    className="shrink-0 rounded border border-zinc-700 px-2 py-1 text-xs hover:bg-zinc-800"
                  >
                    Open ↗
                  </a>
                )}
              </li>
            );
          })}
        </ul>
      </div>
    </section>
  );
}
