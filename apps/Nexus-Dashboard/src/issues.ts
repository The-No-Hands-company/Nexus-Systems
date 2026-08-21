/**
 * Filing issues from inside the ecosystem.
 *
 * The public issue tracker is GitHub, and the front door for it is documented
 * on tnhc.dev. That serves people who already have a GitHub account and know
 * the project exists there. It does not serve the person who is signed in,
 * looking at something broken, and has no reason to own a GitHub account.
 *
 * This closes that gap without adding a second tracker to keep in sync: a
 * signed-in user posts here, and the server files it to the same GitHub repo
 * under a project-owned token. One tracker, two doors.
 *
 * Deliberately server-side. The token never reaches the browser, and the
 * reporter's identity is taken from Auth rather than from anything they typed,
 * so a report cannot be filed in someone else's name.
 */

const GITHUB_API = "https://api.github.com";
const REPO = process.env.NEXUS_ISSUES_REPO || "The-No-Hands-company/Nexus-Systems";

/** Longest body we will forward. GitHub's own limit is 65536. */
const MAX_BODY = 8000;
const MAX_TITLE = 160;

export type IssueReport = {
  title: string;
  body: string;
  /** Which app the reporter was looking at, if the client knows. */
  app?: string;
  /** Page they were on. Recorded to save the "where were you?" round trip. */
  url?: string;
};

export type IssueResult =
  | { ok: true; number: number; url: string }
  | { ok: false; status: number; error: string };

/**
 * Validate a report before it costs a network call.
 *
 * Returns the cleaned report or an error string. Length limits are enforced
 * here rather than trusting the form: the endpoint is reachable by anything
 * that can hold a session, not only by our own UI.
 */
export function validateReport(input: unknown): { report: IssueReport } | { error: string } {
  if (!input || typeof input !== "object") return { error: "expected an object" };
  const r = input as Record<string, unknown>;

  const title = typeof r.title === "string" ? r.title.trim() : "";
  const body = typeof r.body === "string" ? r.body.trim() : "";

  if (!title) return { error: "title is required" };
  if (title.length > MAX_TITLE) return { error: `title must be ${MAX_TITLE} characters or fewer` };
  if (!body) return { error: "description is required" };
  if (body.length > MAX_BODY) return { error: `description must be ${MAX_BODY} characters or fewer` };

  const app = typeof r.app === "string" && r.app.trim() ? r.app.trim().slice(0, 60) : undefined;
  const url = typeof r.url === "string" && r.url.trim() ? r.url.trim().slice(0, 300) : undefined;

  return { report: { title, body, app, url } };
}

/**
 * Render the issue body.
 *
 * The reporter's own words come first and unedited — someone reading the issue
 * should see what was reported before they see our bookkeeping. The context
 * block sits underneath, clearly marked as added by the system so nobody
 * mistakes it for something the reporter typed.
 *
 * The subject is an opaque Auth id, not an email address. It is enough to
 * follow up through the dashboard and does not publish anyone's address on a
 * public issue tracker.
 */
export function renderBody(report: IssueReport, subject: string): string {
  const lines = [
    report.body,
    "",
    "---",
    "",
    "*Filed from the Nexus dashboard.*",
    "",
    `- Reporter: \`${subject}\``,
  ];
  if (report.app) lines.push(`- App: ${report.app}`);
  if (report.url) lines.push(`- Page: ${report.url}`);
  lines.push(`- Received: ${new Date().toISOString()}`);
  return lines.join("\n");
}

/**
 * File the issue.
 *
 * Returns a structured result rather than throwing: the caller turns this into
 * an HTTP response, and a GitHub outage should read as "we could not file
 * this, try again" rather than a 500 with a stack trace.
 */
export async function fileIssue(
  report: IssueReport,
  subject: string,
  token = process.env.NEXUS_ISSUES_TOKEN,
): Promise<IssueResult> {
  if (!token) {
    return {
      ok: false,
      status: 503,
      error: "Issue reporting is not configured on this node (NEXUS_ISSUES_TOKEN is unset).",
    };
  }

  try {
    const res = await fetch(`${GITHUB_API}/repos/${REPO}/issues`, {
      method: "POST",
      headers: {
        authorization: `Bearer ${token}`,
        accept: "application/vnd.github+json",
        "content-type": "application/json",
        "user-agent": "nexus-dashboard-issue-reporter",
      },
      body: JSON.stringify({
        title: report.title,
        body: renderBody(report, subject),
        // Labelled so reports arriving this way are distinguishable from ones
        // filed directly on GitHub, which have different context available.
        labels: ["from-dashboard"],
      }),
      signal: AbortSignal.timeout(10_000),
    });

    if (!res.ok) {
      // GitHub's message is useful to an operator reading logs but should not
      // be relayed verbatim to a browser — it can name the repo and token
      // scopes. The status is enough for the caller to act on.
      return {
        ok: false,
        status: res.status === 401 || res.status === 403 ? 503 : 502,
        error:
          res.status === 401 || res.status === 403
            ? "Issue reporting is misconfigured on this node."
            : "Could not file the issue right now. Please try again.",
      };
    }

    const created = (await res.json()) as { number?: number; html_url?: string };
    if (typeof created.number !== "number" || typeof created.html_url !== "string") {
      return { ok: false, status: 502, error: "GitHub accepted the issue but returned an unexpected response." };
    }
    return { ok: true, number: created.number, url: created.html_url };
  } catch {
    return { ok: false, status: 504, error: "Could not reach the issue tracker. Please try again." };
  }
}
