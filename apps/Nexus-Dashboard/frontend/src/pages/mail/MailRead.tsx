import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import {
  attachmentUrl, markMailSeen, readMailMessage, threadMessages,
  type MailMessage, type MailSummary,
} from "../../api";
import { mailDate, senderName } from "./format";

type State =
  | { kind: "loading" }
  | { kind: "missing" }
  | { kind: "unavailable"; reason: string }
  | { kind: "ready"; message: MailMessage };

export default function MailRead() {
  const { messageId } = useParams<{ messageId: string }>();
  const [state, setState] = useState<State>({ kind: "loading" });
  const [showHtml, setShowHtml] = useState(false);
  const [siblings, setSiblings] = useState<MailSummary[]>([]);

  useEffect(() => {
    if (!messageId) return;
    let cancelled = false;
    setState({ kind: "loading" });

    (async () => {
      try {
        const message = await readMailMessage(messageId);
        if (cancelled) return;
        setState({ kind: "ready", message });
        setShowHtml(false);
        // The rest of the conversation, best effort: failing to load it must
        // not stop the message the reader actually opened from rendering.
        if (message.thread_id) {
          void threadMessages(message.thread_id)
            .then((all) => { if (!cancelled) setSiblings(all.filter((m) => m.id !== message.id)); })
            .catch(() => {});
        } else {
          setSiblings([]);
        }
        // Marking read is best effort: failing to do so must not stop the
        // message being displayed, which is what the reader actually came for.
        void markMailSeen(messageId, true).catch(() => {});
      } catch (e) {
        if (cancelled) return;
        const reason = e instanceof Error ? e.message : "unknown";
        setState(reason === "no such message"
          ? { kind: "missing" }
          : { kind: "unavailable", reason });
      }
    })();

    return () => { cancelled = true; };
  }, [messageId]);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-3xl p-8 text-zinc-500">Loading…</section>;
  }

  if (state.kind === "missing") {
    return (
      <section className="mx-auto max-w-3xl p-8">
        <p className="text-zinc-400">That message is not in your mailbox.</p>
        <Link to="/mail" className="mt-4 inline-block text-sm text-zinc-300 underline">
          Back to mail
        </Link>
      </section>
    );
  }

  if (state.kind === "unavailable") {
    return (
      <section className="mx-auto max-w-3xl p-8">
        <p role="alert" className="text-zinc-400">Mail is unavailable ({state.reason}).</p>
      </section>
    );
  }

  const m = state.message;
  return (
    <section className="mx-auto max-w-3xl space-y-4 p-8">
      <Link to="/mail" className="text-sm text-zinc-400 hover:text-zinc-200">← Mail</Link>

      <header className="border-b border-zinc-800 pb-4">
        <h1 className="text-xl font-semibold text-zinc-100">{m.subject || "(no subject)"}</h1>
        <dl className="mt-2 space-y-0.5 text-sm text-zinc-400">
          <div className="flex gap-2"><dt className="text-zinc-500">From</dt><dd>{m.from}</dd></div>
          {m.to && <div className="flex gap-2"><dt className="text-zinc-500">To</dt><dd>{m.to}</dd></div>}
          {m.date && <div className="flex gap-2"><dt className="text-zinc-500">Date</dt><dd>{m.date}</dd></div>}
        </dl>
      </header>

      {m.html && (
        <div className="flex items-center gap-3 text-xs">
          <button
            type="button"
            onClick={() => setShowHtml((v) => !v)}
            className="rounded border border-zinc-800 px-2 py-1 text-zinc-300 hover:bg-zinc-900"
          >
            {showHtml ? "Show plain text" : "Show formatted"}
          </button>
          {m.blocked_remote && showHtml && (
            <span className="text-zinc-500">
              Remote images were not loaded — opening them tells the sender you read this.
            </span>
          )}
        </div>
      )}

      {showHtml && m.html ? (
        /*
          The HTML is sanitised server-side and then rendered inside a sandboxed
          frame with no script permission and its own CSP. Two layers on
          purpose: sanitisers have been defeated before, and this page is the
          one origin holding the session cookie, so a bypass here is account
          takeover rather than a broken layout. srcdoc keeps it same-document
          without giving it a URL of its own to navigate from.
        */
        <iframe
          title="Message content"
          sandbox=""
          srcDoc={`<!doctype html><meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data:; style-src 'unsafe-inline'">
<style>body{font:14px/1.5 system-ui,sans-serif;color:#e6f2ee;background:transparent;margin:0}
a{color:#27c9a5}blockquote{border-left:2px solid #333;margin:0;padding-left:12px;color:#9db4ad}</style>
${m.html}`}
          className="min-h-[24rem] w-full rounded border border-zinc-800 bg-zinc-950"
        />
      ) : (
        <pre className="whitespace-pre-wrap break-words font-sans text-sm text-zinc-200">
          {m.text}
        </pre>
      )}

      {siblings.length > 0 && (
        <div className="rounded-lg border border-zinc-800 p-4">
          <h2 className="text-xs uppercase tracking-wide text-zinc-500">
            Rest of this conversation
          </h2>
          <ul className="mt-2 space-y-1">
            {siblings.map((s) => (
              <li key={s.id} className="text-sm">
                <Link to={`/mail/m/${s.id}`} className="text-zinc-300 hover:text-zinc-100">
                  {senderName(s.from)}
                  <span className="text-zinc-500"> — {s.subject || "(no subject)"}</span>
                </Link>
                <span className="ml-2 text-xs text-zinc-500">{mailDate(s.received_at)}</span>
              </li>
            ))}
          </ul>
        </div>
      )}

      {m.attachments.length > 0 && (
        <div className="rounded-lg border border-zinc-800 p-4">
          <h2 className="text-xs uppercase tracking-wide text-zinc-500">Attachments</h2>
          <ul className="mt-2 space-y-1">
            {m.attachments.map((a, i) => (
              <li key={`${a.filename}-${i}`} className="text-sm">
                <a
                  href={attachmentUrl(m.id, a.index)}
                  download={a.filename}
                  className="text-zinc-200 underline hover:text-white"
                >
                  {a.filename}
                </a>{" "}
                <span className="text-zinc-500">
                  ({a.mime_type}, {Math.max(1, Math.round(a.size / 1024))} KB)
                </span>
              </li>
            ))}
          </ul>
        </div>
      )}
    </section>
  );
}
