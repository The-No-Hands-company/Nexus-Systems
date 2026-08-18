import { useEffect, useState } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";
import {
  listMailFolders, listMailMessages, searchMail,
  type MailFolder, type MailSummary,
} from "../../api";
import { mailDate, senderName } from "./format";
import MailNav from "./MailNav";

type State =
  | { kind: "loading" }
  | { kind: "unavailable"; reason: string }
  | { kind: "no-mailbox" }
  | { kind: "ready"; folders: MailFolder[]; messages: MailSummary[] };

export default function MailList() {
  const { folderId } = useParams<{ folderId: string }>();
  const [params, setParams] = useSearchParams();
  const query = params.get("q") ?? "";
  const [state, setState] = useState<State>({ kind: "loading" });
  const [draftQuery, setDraftQuery] = useState(query);

  useEffect(() => {
    let cancelled = false;
    setState({ kind: "loading" });

    (async () => {
      try {
        const folders = await listMailFolders();
        // Without an explicit folder, land in the inbox rather than nowhere.
        const target = folderId ?? folders.find((f) => f.kind === "inbox")?.id;
        const messages = query
          ? await searchMail(query)
          : target
            ? await listMailMessages(target)
            : [];
        if (!cancelled) setState({ kind: "ready", folders, messages });
      } catch (e) {
        if (cancelled) return;
        const reason = e instanceof Error ? e.message : "unknown";
        // "No mailbox yet" is an ordinary state for a new account, not a
        // failure, and telling someone their mail is broken when they simply
        // have not been given an address is worse than saying nothing.
        setState(reason === "no mailbox for this account"
          ? { kind: "no-mailbox" }
          : { kind: "unavailable", reason });
      }
    })();

    return () => { cancelled = true; };
  }, [folderId, query]);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-5xl p-8 text-zinc-500">Loading…</section>;
  }

  if (state.kind === "no-mailbox") {
    return (
      <section className="mx-auto max-w-5xl p-8">
        <h1 className="text-2xl font-semibold">Mail</h1>
        <p className="mt-2 text-zinc-400">
          This account does not have a mailbox yet.
        </p>
      </section>
    );
  }

  if (state.kind === "unavailable") {
    return (
      <section className="mx-auto max-w-5xl p-8">
        <h1 className="text-2xl font-semibold">Mail</h1>
        <p role="alert" className="mt-2 text-zinc-400">
          Mail is unavailable right now ({state.reason}).
        </p>
      </section>
    );
  }

  return (
    <section className="mx-auto max-w-5xl space-y-4 p-8">
      <MailNav folders={state.folders} />

      <form
        onSubmit={(e) => {
          e.preventDefault();
          setParams(draftQuery ? { q: draftQuery } : {});
        }}
      >
        <input
          type="search"
          value={draftQuery}
          onChange={(e) => setDraftQuery(e.target.value)}
          placeholder="Search mail"
          aria-label="Search mail"
          className="w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-500"
        />
      </form>

      {state.messages.length === 0 ? (
        <p className="py-12 text-center text-sm text-zinc-500">
          {query ? `Nothing matches “${query}”.` : "Nothing here yet."}
        </p>
      ) : (
        <ul className="divide-y divide-zinc-800 rounded-lg border border-zinc-800">
          {state.messages.map((m) => (
            <li key={m.id}>
              <Link
                to={`/mail/m/${m.id}`}
                className="flex gap-3 px-4 py-3 hover:bg-zinc-900"
              >
                {/* The unread marker is a dot rather than bold-everything, so a
                    full inbox does not read as a wall of emphasis. */}
                <span
                  aria-label={m.seen ? "Read" : "Unread"}
                  className={`mt-2 h-2 w-2 shrink-0 rounded-full ${
                    m.seen ? "bg-transparent" : "bg-zinc-100"
                  }`}
                />
                <span className="min-w-0 flex-1">
                  <span className="flex items-baseline gap-2">
                    <span className={`truncate text-sm ${m.seen ? "text-zinc-400" : "text-zinc-100"}`}>
                      {senderName(m.from)}
                    </span>
                    <span className="ml-auto shrink-0 text-xs text-zinc-500">
                      {mailDate(m.received_at)}
                    </span>
                  </span>
                  <span className={`block truncate text-sm ${m.seen ? "text-zinc-500" : "text-zinc-200"}`}>
                    {m.subject || "(no subject)"}
                  </span>
                  {m.snippet && (
                    <span className="block truncate text-xs text-zinc-500">{m.snippet}</span>
                  )}
                </span>
              </Link>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
