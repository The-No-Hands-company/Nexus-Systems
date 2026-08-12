import { useState } from "react";
import { requestAccess, ApiError } from "../api";

/**
 * The public front door: ask for an account.
 *
 * On success this is the only moment the claim code exists in readable form —
 * the server stores a sha256 of it and there is no endpoint that returns it
 * again. There is also no email to send it in. So the code is rendered here,
 * prominently, with an unambiguous instruction to save it.
 */
export default function RequestAccess() {
  const [username, setUsername] = useState("");
  const [email, setEmail] = useState("");
  const [note, setNote] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [claimCode, setClaimCode] = useState<string | null>(null);

  async function submit() {
    if (!username.trim() || !email.trim()) return;
    setBusy(true);
    setError(null);
    try {
      const result = await requestAccess({
        username: username.trim(),
        email: email.trim(),
        note: note.trim() || undefined,
      });
      setClaimCode(result.claimCode);
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "Something went wrong. Please try again.");
    } finally {
      setBusy(false);
    }
  }

  if (claimCode) {
    return (
      <section className="mx-auto max-w-xl p-8">
        <h1 className="text-2xl font-semibold">Request received</h1>
        <p className="mt-2 text-zinc-400">
          Save the code below. This is the only time it is shown — it cannot be shown again, and
          there is no email being sent. You will need it to finish creating your account once your
          request is approved.
        </p>

        <div className="mt-6 rounded-lg border border-amber-500/40 bg-amber-500/5 p-4">
          <div className="text-xs uppercase tracking-wide text-amber-400">Your claim code</div>
          <code className="mt-2 block break-all font-mono text-lg text-amber-200">{claimCode}</code>
          <button
            type="button"
            onClick={() => void navigator.clipboard?.writeText(claimCode)}
            className="mt-3 rounded border border-zinc-700 px-3 py-1 text-sm hover:bg-zinc-800"
          >
            Copy
          </button>
        </div>

        <p className="mt-6 text-sm text-zinc-500">
          Come back to <span className="text-zinc-300">/claim</span> once you have been approved.
        </p>
      </section>
    );
  }

  return (
    <section className="mx-auto max-w-xl p-8">
      <h1 className="text-2xl font-semibold">Request access</h1>
      <p className="mt-2 text-zinc-400">
        One account gives you every app in the ecosystem. Access is invite-only for now.
      </p>

      <div className="mt-6 space-y-4">
        <div>
          <label htmlFor="username" className="block text-sm text-zinc-400">Username</label>
          <input
            id="username"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-700 bg-zinc-900 px-3 py-2"
          />
        </div>
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
          <label htmlFor="note" className="block text-sm text-zinc-400">
            What would you use it for? <span className="text-zinc-600">(optional)</span>
          </label>
          <textarea
            id="note"
            value={note}
            onChange={(e) => setNote(e.target.value)}
            rows={3}
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
        {busy ? "Sending…" : "Request access"}
      </button>
    </section>
  );
}
