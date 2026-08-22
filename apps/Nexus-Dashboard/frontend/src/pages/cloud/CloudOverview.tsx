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
  tools: CloudTool[] | null;
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
          cloudTools().catch(() => null),
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
    return <section role="status" className="mx-auto max-w-5xl p-8 text-zinc-500">Loading…</section>;
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
  const byId = new Map((tools ?? []).map((t) => [t.id, t]));

  // Flat counters, and peers under `trust`. status.html read status.tools.total
  // / status.users.total / status.peers.total, none of which this response has
  // ever carried, so it displayed 0 everywhere while Cloud reported 86 tools.
  const toolTotal = status.toolCount ?? 0;
  const toolHealthy = status.healthyToolCount ?? 0;
  const toolExposed = status.exposedToolCount ?? 0;
  const peerTotal = status.trust?.peers?.total ?? 0;

  // "Healthy" means a tool sent a heartbeat, which most scaffolds do — they
  // register and report healthy while serving nothing anyone can open. Leading
  // with 36 healthy of 86 told an operator the node was largely working when
  // only 7 tools had an address at all.
  //
  // Reachable is the number worth acting on: registered, healthy, and carrying
  // a public URL. Heartbeating and registered stay visible underneath, so
  // nothing is hidden — only re-ranked by what it actually means.
  const toolReachable = tools === null
    ? null
    : tools.filter((t) => t.publicUrl && (t.health ?? "") === "healthy").length;

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Cloud</h1>
        <p className="mt-2 text-zinc-400">This node's control plane — identity, trust and services.</p>
      </div>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">{toolReachable ?? "—"}</div>
          <div className="text-sm text-zinc-400">Reachable tools</div>
          <div className="mt-1 text-xs text-zinc-500">
            <span>{toolHealthy} heartbeating · </span>
            <span>{toolTotal} registered</span>
          </div>
        </div>
        {/* Was "Users". Cloud has no user count and should not — accounts moved
            to Nexus-Auth. Exposed tools is the number this control plane
            actually owns and the one an operator acts on. */}
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">{toolExposed}</div>
          <div className="text-sm text-zinc-400">Exposed</div>
          <div className="mt-1 text-xs text-zinc-500">reachable publicly</div>
        </div>
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="text-2xl font-semibold text-zinc-100">{peerTotal}</div>
          <div className="text-sm text-zinc-400">Peers</div>
          {/* "federation nodes" alongside 0 implied this node was not one.
              Peers are the *other* nodes; this one is not its own peer. */}
          <div className="mt-1 text-xs text-zinc-500">other nodes federated with</div>
        </div>
      </div>

      <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
        <div className="font-medium text-zinc-100">Nexus Node</div>
        <div className="text-sm text-zinc-500">{identity.shortId || ""}</div>
        <dl className="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-3">
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">DID</dt>
            <dd className="mt-1 break-all font-mono text-sm text-zinc-300">{identity.did || "—"}</dd>
          </div>
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">Short ID</dt>
            <dd className="mt-1 font-mono text-sm text-zinc-300">{identity.shortId || "—"}</dd>
          </div>
          {/* `exampleAddress` is @alice:<shortId> — an illustration of the
              naming scheme, not an address anyone holds. status.html labelled
              it "NS Address", which reads as "this node is @alice". */}
          <div>
            <dt className="text-xs uppercase tracking-wide text-zinc-500">Address format</dt>
            <dd className="mt-1 break-all font-mono text-sm text-zinc-300">
              {identity.exampleAddress || identity.namingScheme || "—"}
            </dd>
          </div>
        </dl>
      </div>

      {trust && (
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
          <div className="flex items-center justify-between">
            <h2 className="text-lg font-medium">Trust lifecycle</h2>
            <span className="text-xs text-zinc-500">updated {timeAgo(trust.updatedAt)}</span>
          </div>
          {/*
            This node, named. The registry counts trust *relationships with
            other nodes*, so it is empty on a standalone node — which read as
            "Nodes (0 total) — none" to an operator looking straight at a node
            that plainly exists. Zero peers is the truth; "no nodes" was not.
          */}
          <div className="mt-3 flex flex-wrap items-baseline gap-x-3 gap-y-1 rounded border border-zinc-800 bg-zinc-950 px-3 py-2">
            <span className="text-xs uppercase tracking-wide text-zinc-500">This node</span>
            <span className="font-mono text-sm text-zinc-200">
              {identity?.shortId ?? "—"}
            </span>
            <span className="text-xs text-zinc-500">
              {status.mode === "standalone"
                ? "standalone — not federating with any other node yet"
                : `mode: ${status.mode ?? "unknown"}`}
            </span>
          </div>

          <div className="mt-3 grid grid-cols-1 gap-4 sm:grid-cols-2">
            <div>
              <div className="text-xs uppercase tracking-wide text-zinc-500">
                Other nodes ({trust.nodes.total ?? 0})
              </div>
              <div className="mt-2"><TrustPills counts={trust.nodes} /></div>
            </div>
            <div>
              <div className="text-xs uppercase tracking-wide text-zinc-500">
                Peers ({trust.peers.total ?? 0})
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
            {tools === null
              ? "Registry unavailable"
              : `${INTERNAL_SERVICES.filter((s) => byId.has(s.id)).length}/${INTERNAL_SERVICES.length} online`}
          </span>
        </div>
        {tools === null ? (
          <p role="status" className="mt-3 text-sm text-zinc-400">
            The service registry is unavailable. Other Cloud information remains current.
          </p>
        ) : (
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
                    aria-label={`Open ${t.name}`}
                    href={t.publicUrl || t.upstreamUrl}
                    target="_blank"
                    rel="noreferrer"
                    className="shrink-0 rounded border border-zinc-700 px-2 py-1 text-xs hover:bg-zinc-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-cyan-400"
                  >
                    Open <span aria-hidden="true">↗</span>
                  </a>
                )}
                </li>
              );
            })}
          </ul>
        )}
      </div>
    </section>
  );
}
