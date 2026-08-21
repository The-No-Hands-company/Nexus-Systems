import { useEffect, useState } from "react";
import { cloudIdentity, type CloudIdentity as CloudIdentityData } from "../../api";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; identity: CloudIdentityData };

/**
 * The ported `view-identity` from Cloud's status.html (loadIdentityView):
 * this node's federation identity.
 *
 * The endpoint returns `exampleAddress`, never `address` — status.html read the
 * missing field and rendered "—" forever. Reading the real field is only half
 * the fix: `@alice:<shortId>` is an *illustration* of the naming scheme, not an
 * address anyone holds, so showing it under "NS address" would state that this
 * node is @alice. It is labelled as the format, with the node's own note
 * alongside it.
 */
export default function CloudIdentity() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudIdentity()
      .then((identity) => { if (!cancelled) setState({ kind: "ready", identity }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Identity</h1>
        <p className="mt-2 text-zinc-400">This node's federation identity.</p>
      </div>

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && (
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2">
          <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
            <div className="text-xs uppercase tracking-wide text-zinc-500">Address format</div>
            <div className="mt-1 break-all font-mono text-sm text-zinc-100">
              {state.identity.exampleAddress || state.identity.namingScheme || "—"}
            </div>
            <p className="mt-2 text-xs text-zinc-500">
              An example, not a registered address — anyone on this node is named this way.
            </p>
          </div>
          <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
            <div className="text-xs uppercase tracking-wide text-zinc-500">Short ID</div>
            <div className="mt-1 font-mono text-sm text-zinc-100">{state.identity.shortId || "—"}</div>
          </div>
          <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4 sm:col-span-2">
            <div className="text-xs uppercase tracking-wide text-zinc-500">
              Decentralized identifier (DID)
            </div>
            <div className="mt-1 break-all font-mono text-sm text-zinc-100">{state.identity.did || "—"}</div>
          </div>
          {state.identity.publicKey && (
            <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-4 sm:col-span-2">
              <div className="text-xs uppercase tracking-wide text-zinc-500">Public key</div>
              <div className="mt-1 break-all font-mono text-sm text-zinc-100">{state.identity.publicKey}</div>
            </div>
          )}
        </div>
      )}
    </section>
  );
}
