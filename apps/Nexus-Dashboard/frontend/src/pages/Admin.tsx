import { useEffect, useState } from "react";
import {
  me, isAdmin, listAccessRequests, decideAccessRequest, createInvite,
  type Me, type AccessRequest,
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

  async function decide(id: string, decision: "approve" | "reject") {
    setError(null);
    try {
      await decideAccessRequest(id, decision);
      // Only drop the row once the server has agreed. Removing it optimistically
      // would tell the operator someone was approved when they were not, and
      // that person could never claim their account.
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

  // Not an operator: render nothing at all rather than an empty admin shell.
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
    </section>
  );
}
