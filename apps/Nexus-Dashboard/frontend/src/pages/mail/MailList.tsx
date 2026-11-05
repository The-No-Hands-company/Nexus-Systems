import { useEffect, useRef, useState } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";
import {
  deleteMailMessage, emptyMailTrash, listMailFolders, listMailMessages, moveMailMessage, searchMail,
  type MailFolder, type MailSummary,
} from "../../api";
import { groupIntoThreads } from "./threads";
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
  const selectedSearchFolder = params.get("folder");
  const selectedUnread = params.get("unread") === "true";
  const searchFolder = selectedSearchFolder === "inbox"
    || selectedSearchFolder === "sent"
    || selectedSearchFolder === "trash"
    || selectedSearchFolder === "junk"
    ? selectedSearchFolder
    : "default";
  const [state, setState] = useState<State>({ kind: "loading" });
  const [draftQuery, setDraftQuery] = useState(query);
  const [draftSearchFolder, setDraftSearchFolder] = useState<
    "default" | "inbox" | "sent" | "trash" | "junk"
  >(searchFolder);
  const [draftUnreadOnly, setDraftUnreadOnly] = useState(selectedUnread);
  const [movingId, setMovingId] = useState<string | null>(null);
  const [deletingId, setDeletingId] = useState<string | null>(null);
  const [moveError, setMoveError] = useState<string | null>(null);
  const [status, setStatus] = useState<string | null>(null);
  const [confirmDeleteId, setConfirmDeleteId] = useState<string | null>(null);
  const [confirmEmptyTrash, setConfirmEmptyTrash] = useState(false);
  const emptyTrashButtonRef = useRef<HTMLButtonElement | null>(null);

  useEffect(() => {
    setDraftQuery(query);
    setDraftSearchFolder(searchFolder);
    setDraftUnreadOnly(selectedUnread);
  }, [query, searchFolder, selectedUnread]);

  const restoreFocus = () => {
    requestAnimationFrame(() => {
      const candidate = document.querySelector<HTMLButtonElement>("[data-mail-action]:not([disabled])");
      if (candidate) {
        candidate.focus();
        return;
      }
      emptyTrashButtonRef.current?.focus();
    });
  };

  useEffect(() => {
    let cancelled = false;
    setState({ kind: "loading" });

    (async () => {
      try {
        const folders = await listMailFolders();
        // Without an explicit folder, land in the inbox rather than nowhere.
        const target = folderId ?? folders.find((f) => f.kind === "inbox")?.id;
        const messages = query
          ? await searchMail(query, {
            folder: searchFolder === "default" ? undefined : searchFolder,
            unread: selectedUnread ? true : undefined,
          })
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
  }, [folderId, query, searchFolder, selectedUnread]);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-5xl px-4 py-6 sm:p-8 text-zinc-500">Loading…</section>;
  }

  if (state.kind === "no-mailbox") {
    return (
      <section className="mx-auto max-w-5xl px-4 py-6 sm:p-8">
        <h1 className="text-2xl font-semibold">Mail</h1>
        <p className="mt-2 text-zinc-400">
          This account does not have a mailbox yet.
        </p>
      </section>
    );
  }

  if (state.kind === "unavailable") {
    return (
      <section className="mx-auto max-w-5xl px-4 py-6 sm:p-8">
        <h1 className="text-2xl font-semibold">Mail</h1>
        <p role="alert" className="mt-2 text-zinc-400">
          Mail is unavailable right now ({state.reason}).
        </p>
      </section>
    );
  }

  const targetFolderId = folderId ?? state.folders.find((f) => f.kind === "inbox")?.id;
  const currentFolder = state.folders.find((f) => f.id === targetFolderId);
  const viewingTrash = !query && currentFolder?.kind === "trash";
  const canMoveToTrash = !query && (currentFolder?.kind === "inbox" || currentFolder?.name === "Junk");
  const canArchive = !query && (currentFolder?.kind === "inbox" || currentFolder?.kind === "sent");

  const moveToTrash = async (id: string) => {
    try {
      setMovingId(id);
      setMoveError(null);
      setStatus(null);
      await moveMailMessage(id, "trash");
      setState((current) => current.kind === "ready"
        ? { ...current, messages: current.messages.filter((message) => message.id !== id) }
        : current);
      setStatus("Message moved to Trash.");
      restoreFocus();
    } catch (e) {
      setMoveError(e instanceof Error ? e.message : "unknown");
    } finally {
      setMovingId(null);
    }
  };

  const restoreFromTrash = async (id: string) => {
    try {
      setMovingId(id);
      setMoveError(null);
      setStatus(null);
      await moveMailMessage(id, "inbox");
      setState((current) => current.kind === "ready"
        ? { ...current, messages: current.messages.filter((message) => message.id !== id) }
        : current);
      setStatus("Message restored to Inbox.");
      setConfirmDeleteId(null);
      restoreFocus();
    } catch (e) {
      setMoveError(e instanceof Error ? e.message : "unknown");
    } finally {
      setMovingId(null);
    }
  };

  const removePermanently = async (id: string) => {
    try {
      setDeletingId(id);
      setMoveError(null);
      setStatus(null);
      await deleteMailMessage(id);
      setState((current) => current.kind === "ready"
        ? { ...current, messages: current.messages.filter((message) => message.id !== id) }
        : current);
      setStatus("Message deleted permanently.");
      setConfirmDeleteId(null);
      restoreFocus();
    } catch (e) {
      setMoveError(e instanceof Error ? e.message : "unknown");
    } finally {
      setDeletingId(null);
    }
  };

  const archiveMessage = async (id: string) => {
    try {
      setMovingId(id);
      setMoveError(null);
      setStatus(null);
      await moveMailMessage(id, "archive");
      setState((current) => current.kind === "ready"
        ? { ...current, messages: current.messages.filter((message) => message.id !== id) }
        : current);
      setStatus("Message archived.");
      restoreFocus();
    } catch (e) {
      setMoveError(e instanceof Error ? e.message : "unknown");
    } finally {
      setMovingId(null);
    }
  };

  const runEmptyTrash = async () => {
    try {
      setMoveError(null);
      setStatus(null);
      await emptyMailTrash();
      setState((current) => current.kind === "ready"
        ? { ...current, messages: [] }
        : current);
      setStatus("Trash emptied permanently.");
      setConfirmEmptyTrash(false);
      restoreFocus();
    } catch (e) {
      setMoveError(e instanceof Error ? e.message : "unknown");
    }
  };

  return (
    <section className="mx-auto max-w-5xl space-y-4 px-4 py-6 sm:p-8">
      <MailNav folders={state.folders} />

      <form
        onSubmit={(e) => {
          e.preventDefault();
          const next = new URLSearchParams();
          const trimmed = draftQuery.trim();
          if (trimmed) {
            next.set("q", trimmed);
            if (draftSearchFolder !== "default") next.set("folder", draftSearchFolder);
            if (draftUnreadOnly) next.set("unread", "true");
          }
          setParams(next);
        }}
        className="grid gap-2 sm:grid-cols-[1fr_auto_auto_auto]"
      >
        <input
          type="search"
          value={draftQuery}
          onChange={(e) => setDraftQuery(e.target.value)}
          placeholder="Search mail"
          aria-label="Search mail"
          className="w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-500"
        />
        <select
          value={draftSearchFolder}
          onChange={(e) =>
            setDraftSearchFolder(e.target.value as "default" | "inbox" | "sent" | "trash" | "junk")}
          aria-label="Search folder"
          className="rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100"
        >
          <option value="default">Inbox + Sent</option>
          <option value="inbox">Inbox</option>
          <option value="sent">Sent</option>
          <option value="junk">Junk</option>
          <option value="trash">Trash</option>
        </select>
        <label className="flex items-center gap-2 rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-300">
          <input
            type="checkbox"
            checked={draftUnreadOnly}
            onChange={(e) => setDraftUnreadOnly(e.target.checked)}
          />
          Unread only
        </label>
        <button
          type="submit"
          className="rounded border border-zinc-700 px-3 py-2 text-sm text-zinc-200 hover:bg-zinc-900"
        >
          Search
        </button>
      </form>
      {viewingTrash && (
        <div className="space-y-2 rounded border border-zinc-800 bg-zinc-900/50 p-3">
          <p className="text-sm text-zinc-400">
            Trash is automatically purged after seven days. Emptying Trash now permanently erases these copies.
          </p>
          {!confirmEmptyTrash ? (
            <button
              ref={emptyTrashButtonRef}
              type="button"
              onClick={() => setConfirmEmptyTrash(true)}
              className="rounded border border-red-700 px-3 py-1.5 text-sm text-red-200 hover:bg-red-950/40"
            >
              Empty Trash
            </button>
          ) : (
            <div className="flex flex-wrap items-center gap-2">
              <span className="text-sm text-zinc-300">This cannot be undone.</span>
              <button
                type="button"
                onClick={() => void runEmptyTrash()}
                className="rounded border border-red-700 px-3 py-1.5 text-sm text-red-200 hover:bg-red-950/40"
              >
                Confirm empty Trash
              </button>
              <button
                type="button"
                onClick={() => setConfirmEmptyTrash(false)}
                className="rounded border border-zinc-700 px-3 py-1.5 text-sm text-zinc-200 hover:bg-zinc-900"
              >
                Cancel
              </button>
            </div>
          )}
        </div>
      )}
      {status && <p role="status" className="text-sm text-zinc-300">{status}</p>}
      {moveError && <p role="alert" className="text-sm text-red-300">Could not move message ({moveError}).</p>}

      {/* Grouped into conversations. A mailing list or a long reply chain
          otherwise fills the list with the same subject repeated, which is
          what made threading the first gap worth closing. */}
      {state.messages.length === 0 ? (
        <p className="py-12 text-center text-sm text-zinc-500">
          {query ? `Nothing matches “${query}”.` : "Nothing here yet."}
        </p>
      ) : (
        <ul className="divide-y divide-zinc-800 rounded-lg border border-zinc-800">
          {groupIntoThreads(state.messages).map(({ latest: m, count, anyUnread }) => (
            <li key={m.thread_id || m.id} className="flex flex-col items-stretch gap-2 p-2 sm:flex-row sm:items-center">
              <Link
                to={`/mail/m/${m.id}`}
                className="flex min-w-0 flex-1 gap-3 rounded px-2 py-2 hover:bg-zinc-900"
              >
                {/* The unread marker is a dot rather than bold-everything, so a
                    full inbox does not read as a wall of emphasis. */}
                <span
                  aria-label={anyUnread ? "Unread" : "Read"}
                  className={`mt-2 h-2 w-2 shrink-0 rounded-full ${
                    anyUnread ? "bg-zinc-100" : "bg-transparent"
                  }`}
                />
                <span className="min-w-0 flex-1">
                  <span className="flex items-baseline gap-2">
                    <span className={`truncate text-sm ${anyUnread ? "text-zinc-100" : "text-zinc-400"}`}>
                      {senderName(m.from)}
                    </span>
                    {count > 1 && (
                      <span
                        aria-label={`${count} messages in this conversation`}
                        className="shrink-0 rounded bg-zinc-800 px-1.5 text-xs text-zinc-400"
                      >
                        {count}
                      </span>
                    )}
                    <span className="ml-auto shrink-0 text-xs text-zinc-500">
                      {mailDate(m.received_at)}
                    </span>
                  </span>
                  <span className={`block truncate text-sm ${anyUnread ? "text-zinc-200" : "text-zinc-500"}`}>
                    {m.subject || "(no subject)"}
                  </span>
                  {m.snippet && (
                    <span className="block truncate text-xs text-zinc-500">{m.snippet}</span>
                  )}
                </span>
              </Link>
              <div className="flex flex-wrap items-center gap-2 px-2 pb-2 sm:px-0 sm:pb-0 sm:pr-2">
                {canArchive && (
                  <button
                    type="button"
                    disabled={movingId === m.id}
                    onClick={() => void archiveMessage(m.id)}
                    aria-label={`Archive ${m.subject || "(no subject)"}`}
                    data-mail-action="archive"
                    className="rounded border border-zinc-700 px-2 py-1 text-xs text-zinc-300 hover:bg-zinc-900 disabled:cursor-wait disabled:opacity-50"
                  >
                    Archive
                  </button>
                )}
                {canMoveToTrash && (
                  <button
                    type="button"
                    disabled={movingId === m.id}
                    onClick={() => void moveToTrash(m.id)}
                    aria-label={`Move ${m.subject || "(no subject)"} to Trash`}
                    data-mail-action="move-trash"
                    className="rounded border border-zinc-700 px-2 py-1 text-xs text-zinc-300 hover:bg-zinc-900 disabled:cursor-wait disabled:opacity-50"
                  >
                    Trash
                  </button>
                )}
                {viewingTrash && (
                  <>
                    <button
                      type="button"
                      disabled={movingId === m.id}
                      onClick={() => void restoreFromTrash(m.id)}
                      data-mail-action="restore"
                      className="rounded border border-zinc-700 px-2 py-1 text-xs text-zinc-300 hover:bg-zinc-900 disabled:cursor-wait disabled:opacity-50"
                    >
                      Restore
                    </button>
                    {confirmDeleteId === m.id ? (
                      <>
                        <button
                          type="button"
                          disabled={deletingId === m.id}
                          onClick={() => void removePermanently(m.id)}
                          data-mail-action="confirm-delete"
                          className="rounded border border-red-700 px-2 py-1 text-xs text-red-200 hover:bg-red-950/40 disabled:cursor-wait disabled:opacity-50"
                        >
                          Confirm delete permanently
                        </button>
                        <button
                          type="button"
                          onClick={() => setConfirmDeleteId(null)}
                          className="rounded border border-zinc-700 px-2 py-1 text-xs text-zinc-300 hover:bg-zinc-900"
                        >
                          Cancel
                        </button>
                      </>
                    ) : (
                      <button
                        type="button"
                        disabled={deletingId === m.id}
                        onClick={() => setConfirmDeleteId(m.id)}
                        data-mail-action="delete"
                        aria-label={`Delete ${m.subject || "(no subject)"} permanently`}
                        className="rounded border border-red-700 px-2 py-1 text-xs text-red-200 hover:bg-red-950/40 disabled:cursor-wait disabled:opacity-50"
                      >
                        Delete permanently
                      </button>
                    )}
                  </>
                )}
              </div>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
