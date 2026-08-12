import { useState } from "react";
import { claimAccount, ApiError } from "../api";

/** The server's machine-readable reasons, said in plain language. */
function explain(reason: string): string {
  switch (reason) {
    case "invalid_code":
      // Deliberately vague on the server side — it returns the same answer for
      // an unknown email and a wrong code so the endpoint cannot be used to
      // discover which addresses have been approved. Do not "helpfully"
      // distinguish them here.
      return "That email and claim code were not recognised as an approved request.";
    case "not_approved":
      return "This request is still pending approval. Check back once it has been approved.";
    case "weak_password":
      return "Choose a password of at least 12 characters.";
    case "too_many_attempts":
      return "Too many attempts. Wait a few minutes and try again.";
    case "network":
      return "Could not reach the server. Check your connection and try again.";
    default:
      return "Something went wrong. Please try again.";
  }
}

/**
 * Redeem a claim code and set a password.
 *
 * The ten recovery codes returned here are the only self-service way back into
 * the account — there is no password-reset email, because there is no outbound
 * email. Losing both the password and these codes means the account is gone
 * and the operator cannot restore it. Continue is therefore gated behind an
 * explicit confirmation rather than being clickable by reflex.
 */
export default function Claim() {
  const [email, setEmail] = useState("");
  const [code, setCode] = useState("");
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [recoveryCodes, setRecoveryCodes] = useState<string[] | null>(null);
  const [saved, setSaved] = useState(false);

  async function submit() {
    if (!email.trim() || !code.trim() || !password) return;
    setBusy(true);
    setError(null);
    try {
      const result = await claimAccount({
        email: email.trim(),
        claimCode: code.trim(),
        password,
      });
      setRecoveryCodes(result.recoveryCodes);
    } catch (err) {
      setError(explain(err instanceof ApiError ? err.reason : "unknown"));
    } finally {
      setBusy(false);
    }
  }

  if (recoveryCodes) {
    return (
      <section className="mx-auto max-w-xl p-8">
        <h1 className="text-2xl font-semibold">Your account is ready</h1>
        <p className="mt-2 text-zinc-400">
          Save these recovery codes somewhere safe. Each one can be used once to get back in if you
          forget your password.
        </p>
        <p className="mt-2 font-medium text-amber-300">
          This is the only time they are shown. If you lose your password and these codes, the
          account cannot be recovered — not by you and not by the operator.
        </p>

        <ul className="mt-6 grid grid-cols-1 gap-2 rounded-lg border border-zinc-800 bg-zinc-900 p-4 sm:grid-cols-2">
          {recoveryCodes.map((c) => (
            <li key={c} className="break-all font-mono text-sm text-zinc-200">{c}</li>
          ))}
        </ul>

        <label className="mt-6 flex items-center gap-2 text-sm">
          <input type="checkbox" checked={saved} onChange={(e) => setSaved(e.target.checked)} />
          I have saved these codes
        </label>

        <button
          type="button"
          disabled={!saved}
          onClick={() => { window.location.href = "/"; }}
          className="mt-4 rounded bg-blue-600 px-4 py-2 font-medium disabled:opacity-40"
        >
          Continue
        </button>
      </section>
    );
  }

  return (
    <section className="mx-auto max-w-xl p-8">
      <h1 className="text-2xl font-semibold">Claim your account</h1>
      <p className="mt-2 text-zinc-400">
        Enter the claim code you were given when you requested access, and choose a password.
      </p>

      <div className="mt-6 space-y-4">
        <div>
          <label htmlFor="email" className="block text-sm text-zinc-400">Email</label>
          <input
            id="email"
            type="email"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2"
          />
        </div>
        <div>
          <label htmlFor="claimCode" className="block text-sm text-zinc-400">Claim code</label>
          <input
            id="claimCode"
            value={code}
            onChange={(e) => setCode(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2 font-mono"
          />
        </div>
        <div>
          <label htmlFor="password" className="block text-sm text-zinc-400">
            Password <span className="text-zinc-600">(at least 12 characters)</span>
          </label>
          <input
            id="password"
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2"
          />
        </div>
      </div>

      {error && <p role="alert" className="mt-4 text-sm text-red-400">{error}</p>}

      <button
        type="button"
        onClick={() => void submit()}
        disabled={busy}
        className="mt-6 rounded bg-blue-600 px-4 py-2 font-medium disabled:opacity-50"
      >
        {busy ? "Claiming…" : "Claim account"}
      </button>
    </section>
  );
}
