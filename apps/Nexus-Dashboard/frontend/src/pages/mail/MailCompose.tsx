import { useState } from "react";
import { Link, useNavigate, useSearchParams } from "react-router-dom";
import { sendMail, type OutgoingAttachment, type SendOutcome } from "../../api";

/// Read a file as base64 without the data: prefix the reader adds.
function readAsBase64(file: File): Promise<OutgoingAttachment> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error(`Could not read ${file.name}`));
    reader.onload = () => {
      const result = String(reader.result);
      // "data:<mime>;base64,<payload>" — only the payload is wanted.
      const comma = result.indexOf(",");
      resolve({
        filename: file.name,
        mime_type: file.type || "application/octet-stream",
        data: comma >= 0 ? result.slice(comma + 1) : result,
      });
    };
    reader.readAsDataURL(file);
  });
}

export default function MailCompose() {
  const navigate = useNavigate();
  // Reply prefills come through the URL so a reply survives a page reload and
  // can be linked to, rather than living in router state that vanishes.
  const [params] = useSearchParams();
  const [to, setTo] = useState(params.get("to") ?? "");
  const [subject, setSubject] = useState(params.get("subject") ?? "");
  const [text, setText] = useState(params.get("quote") ?? "");
  const inReplyTo = params.get("inReplyTo") ?? undefined;
  const [attachments, setAttachments] = useState<OutgoingAttachment[]>([]);
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
      const res = await sendMail({
        to: recipients,
        subject,
        text,
        in_reply_to: inReplyTo,
        attachments: attachments.length ? attachments : undefined,
      });
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

        <div>
          <label className="text-xs uppercase tracking-wide text-zinc-500">
            Attachments
            <input
              type="file"
              multiple
              onChange={async (e) => {
                const files = Array.from(e.target.files ?? []);
                try {
                  const read = await Promise.all(files.map(readAsBase64));
                  setAttachments((prev) => [...prev, ...read]);
                  setError(null);
                } catch (err) {
                  setError(err instanceof Error ? err.message : "Could not read that file.");
                }
                // Clear the input so the same file can be picked again after
                // being removed.
                e.target.value = "";
              }}
              className="mt-1 block w-full text-sm text-zinc-400 file:mr-3 file:rounded file:border-0 file:bg-zinc-800 file:px-3 file:py-1.5 file:text-sm file:text-zinc-200"
            />
          </label>
          {attachments.length > 0 && (
            <ul className="mt-2 space-y-1">
              {attachments.map((a, i) => (
                <li key={`${a.filename}-${i}`} className="flex items-center gap-2 text-sm text-zinc-300">
                  <span className="truncate">{a.filename}</span>
                  <span className="text-xs text-zinc-500">
                    {Math.max(1, Math.round((a.data.length * 0.75) / 1024))} KB
                  </span>
                  <button
                    type="button"
                    onClick={() => setAttachments((prev) => prev.filter((_, n) => n !== i))}
                    aria-label={`Remove ${a.filename}`}
                    className="ml-auto rounded px-2 text-xs text-zinc-500 hover:text-zinc-200"
                  >
                    Remove
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>

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
