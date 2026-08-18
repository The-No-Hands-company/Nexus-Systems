import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { sendMail, type SendOutcome } from "../../api";

export default function MailCompose() {
  const navigate = useNavigate();
  const [to, setTo] = useState("");
  const [subject, setSubject] = useState("");
  const [text, setText] = useState("");
  const [sending, setSending] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [outcomes, setOutcomes] = useState<SendOutcome[] | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const recipients = to.split(",").map((s) => s.trim()).filter(Boolean);
    if (recipients.length === 0) {
      setError("Add at least one recipient.");
      return;
    }

    setSending(true);
    setError(null);
    try {
      const res = await sendMail({ to: recipients, subject, text });
      // Per-recipient outcomes are shown rather than a single "sent": a message
      // can be delivered to three people and rejected for a fourth, and
      // collapsing that into one status hides the part the sender must act on.
      setOutcomes(res.outcomes);
      if (res.outcomes.every((o) => o.status !== "rejected")) {
        setTimeout(() => navigate("/mail"), 1200);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : "Could not send.");
    } finally {
      setSending(false);
    }
  }

  return (
    <section className="mx-auto max-w-3xl space-y-4 p-8">
      <Link to="/mail" className="text-sm text-zinc-400 hover:text-zinc-200">← Mail</Link>
      <h1 className="text-xl font-semibold text-zinc-100">New message</h1>

      <form onSubmit={submit} className="space-y-3">
        <label className="block">
          <span className="text-xs uppercase tracking-wide text-zinc-500">To</span>
          <input
            value={to}
            onChange={(e) => setTo(e.target.value)}
            placeholder="someone@tnhc.dev, other@example.com"
            className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-600"
          />
        </label>

        <label className="block">
          <span className="text-xs uppercase tracking-wide text-zinc-500">Subject</span>
          <input
            value={subject}
            onChange={(e) => setSubject(e.target.value)}
            className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100"
          />
        </label>

        <label className="block">
          <span className="text-xs uppercase tracking-wide text-zinc-500">Message</span>
          <textarea
            value={text}
            onChange={(e) => setText(e.target.value)}
            rows={14}
            className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 font-sans text-sm text-zinc-100"
          />
        </label>

        {error && <p role="alert" className="text-sm text-red-400">{error}</p>}

        {outcomes && (
          <ul className="space-y-1 text-sm" role="status">
            {outcomes.map((o) => (
              <li
                key={o.recipient}
                className={o.status === "rejected" ? "text-red-400" : "text-zinc-400"}
              >
                {o.recipient}: {o.status}
                {o.reason ? ` — ${o.reason}` : ""}
              </li>
            ))}
          </ul>
        )}

        <button
          type="submit"
          disabled={sending}
          className="rounded bg-zinc-100 px-4 py-2 text-sm font-medium text-zinc-900 hover:bg-white disabled:opacity-50"
        >
          {sending ? "Sending…" : "Send"}
        </button>
      </form>
    </section>
  );
}
