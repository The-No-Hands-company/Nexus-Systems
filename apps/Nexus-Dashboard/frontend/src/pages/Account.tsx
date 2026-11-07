import { useEffect, useState } from "react";
import {
  me, listSessions, revokeSession, remainingRecoveryCodes, regenerateRecoveryCodes,
  changePassword, myActivity, logout, ApiError, type Me, type Session, type ActivityEvent,
} from "../api";
import { PageHeader } from "../../../../../packages/nexus-design/src/components/ui/page-header";
import { Badge } from "../../../../../packages/nexus-design/src/components/ui/badge";
import { useTheme, THEMES } from "../shell/useTheme";
import { useDensity } from "../shell/useDensity";

const EVENT_LABELS: Record<string, string> = {
  login_success: "Signed in",
  login_failure: "Wrong password",
  logout: "Signed out",
  password_change: "Password changed",  // pragma: allowlist secret
  session_revoked: "Device signed out",
};

function deviceLabel(deviceId: string): string {
  return `Device ${deviceId.slice(4, 12)}`;
}

function relativeTime(iso: string): string {
  const secs = Math.max(0, Math.round((Date.now() - new Date(iso).getTime()) / 1000));
  if (secs < 60) return "just now";
  const mins = Math.round(secs / 60);
  if (mins < 60) return `${mins} min ago`;
  const hrs = Math.round(mins / 60);
  if (hrs < 24) return `${hrs} h ago`;
  return `${Math.round(hrs / 24)} d ago`;
}

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
/** The codes as a plain-text file in a data: link: no blob URL to revoke, and
 *  the same file the welcome page in Nexus-Auth offers. */
function buildRecoveryCodesDownload(username: string, codes: string[]): string {
  const text = [
    `Nexus recovery codes for ${username}`,
    `Created ${new Date().toISOString().slice(0, 10)} (UTC)`,
    "",
    "Each code signs you in once if you lose your password.",
    "Keep this file somewhere safe and private, and delete it once the codes are stored elsewhere.",
    "",
    ...codes.map((code, i) => `${String(i + 1).padStart(2, " ")}. ${code}`),
    "",
  ].join("\n");
  return `data:text/plain;charset=utf-8,${encodeURIComponent(text)}`;
}

function buildAccountExportDownload(user: Me, sessions: Session[], activity: ActivityEvent[]): string {
  const data = {
    exportDate: new Date().toISOString(),
    account: {
      id: user.id,
      username: user.username,
      email: user.email,
      role: user.role ?? "member",
    },
    activeSessions: sessions.map((s) => ({
      id: s.id,
      deviceId: s.deviceId,
      current: s.current,
      createdAt: s.createdAt,
      expiresAt: s.expiresAt,
    })),
    recentActivity: activity.map((a) => ({
      event: a.event,
      deviceId: a.deviceId,
      at: a.at,
    })),
  };
  return `data:application/json;charset=utf-8,${encodeURIComponent(JSON.stringify(data, null, 2))}`;
}

export default function Account() {
  const [user, setUser] = useState<Me | null>(null);
  const [sessions, setSessions] = useState<Session[]>([]);
  const [activity, setActivity] = useState<ActivityEvent[]>([]);
  const [remaining, setRemaining] = useState<number | null>(null);

  const [current, setCurrent] = useState("");
  const [next, setNext] = useState("");
  const [pwError, setPwError] = useState<string | null>(null);
  const [pwDone, setPwDone] = useState(false);

  const [newCodes, setNewCodes] = useState<string[] | null>(null);
  const [savedCodes, setSavedCodes] = useState(false);

  const [theme, setTheme] = useTheme();
  const [density, setDensity] = useDensity();
  const [loggingOutCurrent, setLoggingOutCurrent] = useState(false);
  const [revokingOthers, setRevokingOthers] = useState(false);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const [u, s, r, a] = await Promise.all([
        me(),
        listSessions().catch(() => [] as Session[]),
        remainingRecoveryCodes().catch(() => null),
        myActivity().catch(() => [] as ActivityEvent[]),
      ]);
      if (cancelled) return;
      setUser(u);
      setSessions(s);
      setRemaining(r);
      setActivity(a);
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

  async function revokeOtherSessions() {
    setRevokingOthers(true);
    const others = sessions.filter((s) => !s.current);
    try {
      await Promise.all(others.map((s) => revokeSession(s.id)));
      setSessions((list) => list.filter((s) => s.current));
    } catch {
      listSessions().then(setSessions).catch(() => {});
    } finally {
      setRevokingOthers(false);
    }
  }

  async function handleSignOutThisDevice() {
    setLoggingOutCurrent(true);
    await logout();
  }

  if (!user) {
    return <section className="mx-auto max-w-2xl px-4 py-6 sm:p-8 text-zinc-500">Loading…</section>;
  }

  return (
    <section className="mx-auto max-w-2xl">
      <PageHeader
        title="Account"
        description={`Signed in as ${user.username} · ${user.email}`}
        actions={<Badge variant="accent">{user.role ?? "member"}</Badge>}
      />
      <div className="space-y-10 px-6 py-6">

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
            {/* One per row and never wrapped: a code split across lines is one
                nobody can copy correctly. Tapping the list selects all of it. */}
            <ol
              aria-label="Recovery codes"
              className="mt-3 list-decimal select-all overflow-x-auto rounded-lg border border-zinc-800 bg-zinc-900 py-3 pl-10 pr-4 marker:text-zinc-500"
            >
              {newCodes.map((c) => (
                <li key={c} className="whitespace-nowrap font-mono text-[clamp(0.7rem,3.4vw,0.875rem)] leading-7 text-zinc-200">{c}</li>
              ))}
            </ol>
            <a
              href={buildRecoveryCodesDownload(user?.username ?? "account", newCodes)}
              download={`nexus-recovery-codes-${(user?.username ?? "account").toLowerCase().replace(/[^a-z0-9]/g, "")}.txt`}
              className="mt-3 inline-block rounded border border-blue-500 px-4 py-2 text-sm text-blue-200 hover:bg-blue-950/40"
            >
              Download as a text file
            </a>
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

      {/* Appearance & Interface Preferences */}
      <div>
        <h2 className="text-lg font-medium">Appearance & Preferences</h2>
        <p className="mt-1 text-sm text-zinc-400">
          Personalise your Nexus console theme and interface density. Settings are saved locally.
        </p>

        <div className="mt-4 space-y-4 rounded-lg border border-zinc-800 bg-zinc-950/40 p-4">
          <div>
            <span className="block text-xs font-medium uppercase tracking-wider text-zinc-400 mb-2">
              Color Palette
            </span>
            <div className="grid grid-cols-2 gap-2 sm:grid-cols-4">
              {THEMES.map((t) => (
                <button
                  key={t.id}
                  type="button"
                  onClick={() => setTheme(t.id)}
                  aria-pressed={theme === t.id}
                  className={`flex items-center gap-2.5 rounded-lg border px-3 py-2 text-sm transition-colors text-left ${
                    theme === t.id
                      ? "border-accent bg-accent/15 text-accent font-medium shadow-sm"
                      : "border-zinc-800 bg-zinc-900/60 text-zinc-400 hover:border-zinc-700 hover:text-zinc-200"
                  }`}
                >
                  <span
                    className="h-3.5 w-3.5 rounded-full border border-white/20 shrink-0"
                    style={{ backgroundColor: t.hex }}
                    aria-hidden="true"
                  />
                  <span className="truncate">{t.name}</span>
                </button>
              ))}
            </div>
          </div>

          <div className="border-t border-zinc-800/80 pt-3 flex flex-wrap items-center justify-between gap-3">
            <div>
              <span className="block text-xs font-medium uppercase tracking-wider text-zinc-400">
                Display Density
              </span>
              <p className="text-xs text-zinc-500">
                Adjust padding and row heights across the shell.
              </p>
            </div>
            <div className="inline-flex rounded-lg border border-zinc-800 bg-zinc-900 p-0.5">
              <button
                type="button"
                onClick={() => setDensity("balanced")}
                aria-pressed={density === "balanced"}
                className={`rounded-md px-3 py-1 text-xs font-medium transition-colors ${
                  density === "balanced"
                    ? "bg-zinc-800 text-zinc-100 shadow-sm"
                    : "text-zinc-400 hover:text-zinc-200"
                }`}
              >
                Balanced
              </button>
              <button
                type="button"
                onClick={() => setDensity("compact")}
                aria-pressed={density === "compact"}
                className={`rounded-md px-3 py-1 text-xs font-medium transition-colors ${
                  density === "compact"
                    ? "bg-zinc-800 text-zinc-100 shadow-sm"
                    : "text-zinc-400 hover:text-zinc-200"
                }`}
              >
                Compact
              </button>
            </div>
          </div>
        </div>
      </div>

      <div>
        <div className="flex flex-wrap items-center justify-between gap-2">
          <div>
            <h2 className="text-lg font-medium">Active sessions</h2>
            <p className="mt-1 text-xs text-zinc-400">
              Devices currently signed in to your account across the Nexus mesh.
            </p>
          </div>
          {sessions.some((s) => !s.current) && (
            <button
              type="button"
              disabled={revokingOthers}
              onClick={() => void revokeOtherSessions()}
              className="rounded-md border border-zinc-700 bg-zinc-900 px-3 py-1.5 text-xs font-medium text-zinc-300 hover:bg-zinc-800 hover:text-white transition-colors disabled:opacity-50"
            >
              {revokingOthers ? "Revoking…" : "Sign out all other devices"}
            </button>
          )}
        </div>

        {sessions.length === 0 ? (
          <p className="mt-3 text-sm text-zinc-400">No other sessions.</p>
        ) : (
          <ul className="mt-3 divide-y divide-zinc-800 rounded-lg border border-zinc-800">
            {sessions.map((s) => (
              <li key={s.id} className="flex items-center justify-between p-3">
                <div className="text-sm">
                  <div className="text-zinc-200">
                    {deviceLabel(s.deviceId)}
                    {s.current && <span className="ml-2 text-xs text-emerald-400">This device</span>}
                  </div>
                  <div className="text-zinc-500">{relativeTime(s.createdAt)}</div>
                </div>
                {s.current ? (
                  <button
                    type="button"
                    disabled={loggingOutCurrent}
                    onClick={() => void handleSignOutThisDevice()}
                    className="rounded border border-red-900/60 bg-red-950/20 px-3 py-1 text-sm text-red-400 hover:bg-red-950/50 hover:text-red-300 transition-colors disabled:opacity-50"
                  >
                    {loggingOutCurrent ? "Signing out…" : "Sign out of this device"}
                  </button>
                ) : (
                  <button
                    type="button"
                    onClick={() => void revoke(s.id)}
                    className="rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800 transition-colors"
                  >
                    Sign this device out
                  </button>
                )}
              </li>
            ))}
          </ul>
        )}
      </div>

      <div>
        <h2 className="text-lg font-medium">Your recent activity</h2>
        {activity.length === 0 ? (
          <p className="mt-2 text-sm text-zinc-400">Nothing in the last 30 days.</p>
        ) : (
          <ul className="mt-3 divide-y divide-zinc-800 rounded-lg border border-zinc-800">
            {activity.map((e, i) => (
              <li key={i} className="flex items-center justify-between p-3 text-sm">
                <div>
                  <span className="text-zinc-200">{EVENT_LABELS[e.event] ?? e.event}</span>
                  {e.deviceId ? (
                    <span className="ml-2 text-zinc-500">{deviceLabel(e.deviceId)}</span>
                  ) : e.event === "login_failure" ? (
                    <span className="ml-2 text-zinc-500">on an unknown device</span>
                  ) : null}
                </div>
                <span className="text-zinc-500">{relativeTime(e.at)}</span>
              </li>
            ))}
          </ul>
        )}
      </div>

      {/* Data Sovereignty & Export */}
      <div>
        <h2 className="text-lg font-medium">Data & Sovereignty</h2>
        <p className="mt-1 text-sm text-zinc-400">
          Nexus does not track your telemetry or monetize user data. All profile, session, and audit logs stay on your local node.
        </p>
        <div className="mt-4 flex flex-wrap items-center gap-3">
          <a
            href={buildAccountExportDownload(user, sessions, activity)}
            download={`nexus-account-export-${user.username.toLowerCase().replace(/[^a-z0-9]/g, "")}-${new Date().toISOString().slice(0, 10)}.json`}
            className="inline-flex items-center gap-2 rounded-md border border-zinc-700 bg-zinc-900 px-4 py-2 text-sm font-medium text-zinc-200 hover:bg-zinc-800 transition-colors"
          >
            <svg className="h-4 w-4 text-zinc-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
            </svg>
            Export account data (.json)
          </a>
          <button
            type="button"
            disabled={loggingOutCurrent}
            onClick={() => void handleSignOutThisDevice()}
            className="inline-flex items-center gap-2 rounded-md border border-red-900/60 bg-red-950/30 px-4 py-2 text-sm font-medium text-red-400 hover:bg-red-950/60 hover:text-red-300 transition-colors disabled:opacity-50"
          >
            Sign out of Nexus
          </button>
        </div>
      </div>
      </div>
    </section>
  );
}
