import { useState } from "react";
import { reportIssue, ApiError, type FiledIssue } from "../api";

function explain(reason: string): string {
  switch (reason) {
    case "not_authenticated":
      return "Your session expired. Sign in again and your report will go through.";
    case "network":
      return "Could not reach the server. Try again in a moment.";
    default:
      // The server sends human-readable text for validation and upstream
      // failures, so showing it directly is better than mapping every case.
      return reason;
  }
}

/**
 * Report a problem, without needing a GitHub account.
 *
 * The public tracker is GitHub and stays the single source of truth — this
 * files into the same repository rather than starting a second queue that
 * would need reconciling. What it removes is the requirement to own a GitHub
 * account and know the project lives there, which is most people who would
 * ever hit a bug here.
 *
 * The reporter is never asked who they are. The server takes that from the
 * session, so a report cannot be filed under someone else's name, and the
 * issue records an opaque subject rather than an email address — a public
 * tracker should not publish anyone's address.
 */
export default function ReportIssue() {
  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [filed, setFiled] = useState<FiledIssue | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    setBusy(true);
    try {
      const result = await reportIssue({
        title,
        body,
        // Where they were when they hit it, so nobody has to ask.
        url: window.location.href,
      });
      setFiled(result);
      setTitle("");
      setBody("");
    } catch (err) {
      setError(explain(err instanceof ApiError ? err.reason : "unknown"));
    } finally {
      setBusy(false);
    }
  }

  if (filed) {
    return (
      <div className="mx-auto max-w-2xl p-8">
        <h1 className="text-2xl font-semibold text-text-primary">Thank you — that is filed.</h1>
        <p className="mt-4 text-text-muted">
          It is issue #{filed.number} on the public tracker. You can follow it
          there, or not — either way it has been received.
        </p>
        <div className="mt-6 flex gap-3">
          <a
            href={filed.url}
            target="_blank"
            rel="noreferrer noopener"
            className="rounded border border-border px-4 py-2 text-sm text-text-primary hover:bg-surface"
          >
            View issue #{filed.number}
          </a>
          <button
            type="button"
            onClick={() => setFiled(null)}
            className="rounded border border-border px-4 py-2 text-sm text-text-muted hover:bg-surface"
          >
            Report something else
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="mx-auto max-w-2xl p-8">
      <h1 className="text-2xl font-semibold text-text-primary">Report a problem</h1>
      <p className="mt-3 text-text-muted">
        Anything that looks wrong, broken, missing or merely strange. This goes
        straight to the public issue tracker — you do not need a GitHub account,
        and you do not need to be sure it is a real bug.
      </p>
      <p className="mt-2 text-sm text-text-muted">
        Everything here is written by AI, which makes an outside pair of eyes
        worth more rather than less. A report that turns out to be nothing costs
        us a few minutes; a problem nobody mentions costs a great deal more.
      </p>

      <form onSubmit={submit} className="mt-8 space-y-5">
        <div>
          <label htmlFor="issue-title" className="block text-sm text-text-muted">
            What happened?
          </label>
          <input
            id="issue-title"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            maxLength={160}
            required
            placeholder="Deploy button does nothing on the sites page"
            className="mt-2 w-full rounded border border-border bg-surface px-3 py-2 text-text-primary"
          />
        </div>

        <div>
          <label htmlFor="issue-body" className="block text-sm text-text-muted">
            Any detail you can give
          </label>
          <textarea
            id="issue-body"
            value={body}
            onChange={(e) => setBody(e.target.value)}
            maxLength={8000}
            required
            rows={9}
            placeholder="What you were doing, what you expected, what happened instead. Rough notes are fine."
            className="mt-2 w-full rounded border border-border bg-surface px-3 py-2 font-mono text-sm text-text-primary"
          />
          <p className="mt-1 text-xs text-text-muted">
            The page you are on is attached automatically. Your email address is
            not — the issue records an internal id, not your address.
          </p>
        </div>

        {error && (
          <p role="alert" className="text-sm text-red-400">
            {error}
          </p>
        )}

        <button
          type="submit"
          disabled={busy || !title.trim() || !body.trim()}
          className="rounded bg-accent px-5 py-2 text-sm font-medium text-void disabled:opacity-50"
        >
          {busy ? "Filing…" : "Send report"}
        </button>
      </form>
    </div>
  );
}
