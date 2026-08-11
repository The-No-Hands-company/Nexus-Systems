import { useEffect, useState } from "react";
import {
  me, listSessions, revokeSession, remainingRecoveryCodes, regenerateRecoveryCodes,
  changePassword, ApiError, type Me, type Session,
} from "../api";

function explainPassword(reason: string): string {
  switch (reason) {
    case "weak_password":
      return "Choose a new password of at least 12 characters.";
    case "password change failed":
      return "That current password is not correct.";
    case "forbidden":
      return "You are not allowed to change this password.";
    case "network":
      return "Could not reach the server. Try again in a moment.";
    default:
      return "Could not change the password. Please try again.";
  }
}

/**
 * The signed-in user's own account.
 *
 * Everything here is self-service and scoped to the caller: their password,
 * their recovery codes, their sessions. There is deliberately no way to act on
 * another account from this page — administration lives behind the admin panel
 * and its own permission.
 */
export default function Account() {
  const [user, setUser] = useState<Me | null>(null);
  const [sessions, setSessions] = useState<Session[]>([]);
  const [remaining, setRemaining] = useState<number | null>(null);

  const [current, setCurrent] = useState("");
  const [next, setNext] = useState("");
  const [pwError, setPwError] = useState<string | null>(null);
  const [pwDone, setPwDone] = useState(false);

  const [newCodes, setNewCodes] = useState<string[] | null>(null);
  const [savedCodes, setSavedCodes] = useState(false);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const [u, s, r] = await Promise.all([
        me(),
        listSessions().catch(() => [] as Session[]),
        remainingRecoveryCodes().catch(() => null),
      ]);
      if (cancelled) return;
      setUser(u);
      setSessions(s);
      setRemaining(r);
    })();
    return () => { cancelled = true; };
  }, []);

  async function submitPassword() {
    if (!user || !current || !next) return;
    setPwError(null);
    setPwDone(false);
    try {
      await changePassword(user.id, current, next);
      setPwDone(true);
      setCurrent("");
      setNext("");
    } catch (err) {
      setPwError(explainPassword(err instanceof ApiError ? err.reason : "unknown"));
    }
  }

  async function regenerate() {
    try {
      const codes = await regenerateRecoveryCodes();
      setNewCodes(codes);
      setSavedCodes(false);
      setRemaining(codes.length);
    } catch {
      setPwError("Could not regenerate recovery codes. Try again in a moment.");
    }
  }

  async function revoke(id: string) {
    try {
      await revokeSession(id);
      setSessions((list) => list.filter((s) => s.id !== id));
    } catch {
      // Leave the row in place — pretending it is gone when the server still
      // honours it would be worse than showing the failure on the next load.
    }
  }

  if (!user) {
    return <section className="mx-auto max-w-2xl p-8 text-zinc-500">Loading…</section>;
  }

  return (
    <section className="mx-auto max-w-2xl space-y-10 p-8">
      <div>
        <h1 className="text-2xl font-semibold">Account</h1>
        <p className="mt-2 text-zinc-400">
          Signed in as <span className="text-zinc-100">{user.username}</span>{" "}
          <span className="text-zinc-500">{user.email}</span>
        </p>
      </div>

      <div>
        <h2 className="text-lg font-medium">Password</h2>
        <div className="mt-3 space-y-3">
          <div>
            <label htmlFor="current" className="block text-sm text-zinc-400">Current password</label>
            <input
              id="current" type="password" value={current}
              onChange={(e) => setCurrent(e.target.value)}
              className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2"
            />
          </div>
          <div>
            <label htmlFor="next" className="block text-sm text-zinc-400">
              New password <span className="text-zinc-600">(at least 12 characters)</span>
            </label>
            <input
              id="next" type="password" value={next}
              onChange={(e) => setNext(e.target.value)}
              className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2"
            />
          </div>
        </div>
        {pwError && <p role="alert" className="mt-3 text-sm text-red-400">{pwError}</p>}
        {pwDone && <p className="mt-3 text-sm text-green-400">Password changed.</p>}
        <button
          type="button" onClick={() => void submitPassword()}
          className="mt-4 rounded bg-blue-600 px-4 py-2 font-medium"
        >
          Change password
        </button>
      </div>

      <div>
        <h2 className="text-lg font-medium">Recovery codes</h2>
        <p className="mt-2 text-sm text-zinc-400">
          {remaining === null ? "Unknown" : `${remaining} unused`} — these are the only way back in
          if you forget your password. Regenerating issues a fresh set and the previous codes stop
          working immediately.
        </p>

        {newCodes ? (
          <div className="mt-4">
            <p className="font-medium text-amber-300">
              Save these now. This is the only time they are shown.
            </p>
            <ul className="mt-3 grid grid-cols-1 gap-2 rounded-lg border border-zinc-800 bg-zinc-900 p-4 sm:grid-cols-2">
              {newCodes.map((c) => (
                <li key={c} className="break-all font-mono text-sm text-zinc-200">{c}</li>
              ))}
            </ul>
            <label className="mt-3 flex items-center gap-2 text-sm">
              <input
                type="checkbox" checked={savedCodes}
                onChange={(e) => setSavedCodes(e.target.checked)}
              />
              I have saved these codes
            </label>
            <button
              type="button" disabled={!savedCodes} onClick={() => setNewCodes(null)}
              className="mt-3 rounded bg-blue-600 px-4 py-2 font-medium disabled:opacity-40"
            >
              Done
            </button>
          </div>
        ) : (
          <button
            type="button" onClick={() => void regenerate()}
            className="mt-3 rounded border border-zinc-700 px-4 py-2"
          >
            Regenerate codes
          </button>
        )}
      </div>

      <div>
        <h2 className="text-lg font-medium">Active sessions</h2>
        {sessions.length === 0 ? (
          <p className="mt-2 text-sm text-zinc-400">No other sessions.</p>
        ) : (
          <ul className="mt-3 divide-y divide-zinc-800 rounded-lg border border-zinc-800">
            {sessions.map((s) => (
              <li key={s.id} className="flex items-center justify-between p-3">
                <div className="text-sm">
                  <div className="text-zinc-200">{s.ipAddress}</div>
                  <div className="text-zinc-500">{s.userAgent}</div>
                </div>
                <button
                  type="button" onClick={() => void revoke(s.id)}
                  className="rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800"
                >
                  Revoke
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}
