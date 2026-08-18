import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { markMailSeen, readMailMessage, type MailMessage } from "../../api";

type State =
  | { kind: "loading" }
  | { kind: "missing" }
  | { kind: "unavailable"; reason: string }
  | { kind: "ready"; message: MailMessage };

export default function MailRead() {
  const { messageId } = useParams<{ messageId: string }>();
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    if (!messageId) return;
    let cancelled = false;
    setState({ kind: "loading" });

    (async () => {
      try {
        const message = await readMailMessage(messageId);
        if (cancelled) return;
        setState({ kind: "ready", message });
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

      {/*
        The plain-text part is rendered, never the HTML one. Injecting a
        stranger's HTML into this page would hand them script execution on the
        shell's own origin — the one place in the ecosystem where a session
        cookie lives. Rendering HTML mail safely needs sandboxing and sanitising
        that does not exist yet, and until it does, text is the honest option.
      */}
      <pre className="whitespace-pre-wrap break-words font-sans text-sm text-zinc-200">
        {m.text}
      </pre>

      {m.html && !m.text.trim() && (
        <p className="text-xs text-zinc-500">
          This message has only an HTML body, which is not displayed yet.
        </p>
      )}

      {m.attachments.length > 0 && (
        <div className="rounded-lg border border-zinc-800 p-4">
          <h2 className="text-xs uppercase tracking-wide text-zinc-500">Attachments</h2>
          <ul className="mt-2 space-y-1">
            {m.attachments.map((a, i) => (
              <li key={`${a.filename}-${i}`} className="text-sm text-zinc-300">
                {a.filename}{" "}
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
