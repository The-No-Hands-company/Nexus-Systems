import { useEffect, useState, useCallback } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";

interface RepoInfo { name: string; description?: string; visibility: string; cloneUrl?: string; }
interface BranchInfo { branches: string[]; default: string | null; empty: boolean; }
interface TreeEntry { name: string; type: "tree" | "blob"; size: number | null; }
interface Commit { sha: string; shortSha: string; message: string; author: string; date: string; }
interface BlobResult { content: string | null; encoding: string; path: string; }
interface Issue { id: number; number: number; title: string; body: string; state: "open" | "closed"; author: string | null; assignee: string | null; labels: string[]; created_at: string; updated_at: string; }
interface IssueList { issues: Issue[]; open: number; closed: number; }

function relativeTime(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  const mins = Math.floor(diff / 60000);
  if (mins < 1) return "just now";
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  const days = Math.floor(hrs / 24);
  if (days < 30) return `${days}d ago`;
  return new Date(iso).toLocaleDateString();
}

function FolderIcon() {
  return <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M1 3.5A1.5 1.5 0 0 1 2.5 2h3.671a1.5 1.5 0 0 1 1.06.44l.47.47H13.5A1.5 1.5 0 0 1 15 4.5v7a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 1 11.5v-8z"/></svg>;
}

function FileIcon() {
  return <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M4 0a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V5.414A2 2 0 0 0 13.414 4L10 .586A2 2 0 0 0 8.586 0H4zm4 1.5V4a1 1 0 0 0 1 1h2.5L8 1.5z"/></svg>;
}

function BookIcon() {
  return <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor"><path d="M1 2.828A2 2 0 0 1 2.828 1h4.344a2 2 0 0 1 1.414.586l.028.028A2 2 0 0 1 9 3v8.172A2 2 0 0 1 7.172 13H2.828A2 2 0 0 1 1 11.172V2.828zm9 0A2 2 0 0 1 11.828 1h.344A2 2 0 0 1 14 2.828v8.344A2 2 0 0 1 12.172 13H11.5a.5.5 0 0 1 0-1h.672A1 1 0 0 0 13 11.172V2.828A1 1 0 0 0 12.172 2h-.344A1 1 0 0 0 11 2.828V12.5a.5.5 0 0 1-1 0V2.828z"/></svg>;
}

function IssueOpenIcon() {
  return <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M8 9.5a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3z"/><path d="M8 0a8 8 0 1 1 0 16A8 8 0 0 1 8 0zM1.5 8a6.5 6.5 0 1 0 13 0 6.5 6.5 0 0 0-13 0z"/></svg>;
}

function IssueClosedIcon() {
  return <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M11.28 6.78a.75.75 0 0 0-1.06-1.06L7.25 8.69 5.78 7.22a.75.75 0 0 0-1.06 1.06l2 2a.75.75 0 0 0 1.06 0l3.5-3.5z"/><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zm-1.5 0a6.5 6.5 0 1 0-13 0 6.5 6.5 0 0 0 13 0z"/></svg>;
}

export default function RepoDetail() {
  const { name } = useParams<{ name: string }>();
  const [searchParams, setSearchParams] = useSearchParams();

  const tab = searchParams.get("tab") ?? "code";
  const rawRef = searchParams.get("ref") ?? "";
  const path = searchParams.get("path") ?? "";
  const file = searchParams.get("file") ?? "";

  const [repo, setRepo] = useState<RepoInfo | null>(null);
  const [branchInfo, setBranchInfo] = useState<BranchInfo | null>(null);
  const [tree, setTree] = useState<TreeEntry[] | null>(null);
  const [blob, setBlob] = useState<BlobResult | null>(null);
  const [commits, setCommits] = useState<Commit[] | null>(null);
  const [issueData, setIssueData] = useState<IssueList | null>(null);
  const [issueState, setIssueState] = useState<"open" | "closed">("open");
  const [showNewIssue, setShowNewIssue] = useState(false);
  const [readme, setReadme] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);

  const ref = rawRef || branchInfo?.default || "HEAD";

  const navigate = useCallback(
    (params: Record<string, string>) => {
      const next = new URLSearchParams(searchParams);
      for (const [k, v] of Object.entries(params)) {
        if (v) next.set(k, v);
        else next.delete(k);
      }
      setSearchParams(next, { replace: false });
    },
    [searchParams, setSearchParams],
  );

  // Load repo meta + branches once
  useEffect(() => {
    if (!name) return;
    setLoading(true);
    setError(null);
    Promise.all([
      fetch(`/api/repos/${name}`).then((r) => r.json() as Promise<RepoInfo>),
      fetch(`/api/repos/${name}/branches`).then((r) => r.json() as Promise<BranchInfo>),
    ])
      .then(([repoData, branchData]) => {
        setRepo(repoData);
        setBranchInfo(branchData);
        setLoading(false);
      })
      .catch(() => { setError("Failed to load repository"); setLoading(false); });
  }, [name]);

  // Load content based on tab / file / path
  useEffect(() => {
    if (!name || !branchInfo || loading) return;

    const effectiveRef = rawRef || branchInfo.default || "HEAD";

    if (branchInfo.empty) {
      setTree(null); setBlob(null); setCommits(null);
      return;
    }

    if (tab === "commits") {
      setCommits(null);
      fetch(`/api/repos/${name}/commits?ref=${encodeURIComponent(effectiveRef)}&limit=50`)
        .then((r) => r.json())
        .then((d) => setCommits(d.commits ?? []));
      return;
    }

    if (tab === "issues") {
      setIssueData(null);
      fetch(`/api/repos/${name}/issues?state=${issueState}`)
        .then((r) => r.json())
        .then((d) => setIssueData(d));
      return;
    }

    // Code tab
    if (file) {
      setBlob(null);
      fetch(`/api/repos/${name}/blob?ref=${encodeURIComponent(effectiveRef)}&path=${encodeURIComponent(file)}`)
        .then((r) => r.json())
        .then((d) => setBlob(d));
      setTree(null);
      return;
    }

    setBlob(null);
    fetch(`/api/repos/${name}/tree?ref=${encodeURIComponent(effectiveRef)}&path=${encodeURIComponent(path)}`)
      .then((r) => r.json())
      .then((d) => setTree(d.tree ?? []));

    if (!path) {
      // Try to load README at root
      fetch(`/api/repos/${name}/blob?ref=${encodeURIComponent(effectiveRef)}&path=README.md`)
        .then((r) => r.json())
        .then((d) => { if (d.content) setReadme(d.content); })
        .catch(() => {});
    } else {
      setReadme(null);
    }
  }, [name, branchInfo, loading, tab, rawRef, path, file, issueState]);

  function copyCloneUrl() {
    if (!repo?.cloneUrl) return;
    navigator.clipboard.writeText(repo.cloneUrl).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 1800);
    });
  }

  if (loading) return <div className="app-content"><p className="loading">Loading…</p></div>;
  if (error || !repo) return <div className="app-content"><p className="error-msg">{error ?? "Not found"}</p></div>;

  const pathParts = (file || path).split("/").filter(Boolean);
  const isViewingFile = !!file;
  const displayedPath = file || path;

  return (
    <div className="app-content">
      {/* Repo header */}
      <div className="repo-header">
        <div className="repo-name-row">
          <Link to="/" style={{ color: "var(--muted)", fontSize: 13 }}>Repositories</Link>
          <span style={{ color: "var(--faint)" }}>/</span>
          <span className="repo-name">{repo.name}</span>
          <span className={`badge badge-${repo.visibility}`}>{repo.visibility}</span>
        </div>
        {repo.description && <p className="repo-desc">{repo.description}</p>}
        {repo.cloneUrl && (
          <div className="clone-box">
            <span className="clone-label">HTTPS</span>
            <span className="clone-url">{repo.cloneUrl}</span>
            <button className="clone-copy" onClick={copyCloneUrl}>
              {copied ? "Copied!" : "Copy"}
            </button>
          </div>
        )}
      </div>

      {/* Toolbar: branch selector + tabs */}
      <div className="repo-toolbar">
        {!branchInfo?.empty && branchInfo && branchInfo.branches.length > 0 && (
          <select
            className="branch-select"
            value={rawRef || branchInfo.default || ""}
            onChange={(e) => navigate({ ref: e.target.value, path: "", file: "" })}
          >
            {branchInfo.branches.map((b) => (
              <option key={b} value={b}>{b}</option>
            ))}
          </select>
        )}
        <div className="repo-tabs">
          <button
            className={`repo-tab${tab === "code" ? " active" : ""}`}
            onClick={() => navigate({ tab: "", path: "", file: "" })}
          >
            Code
          </button>
          <button
            className={`repo-tab${tab === "commits" ? " active" : ""}`}
            onClick={() => navigate({ tab: "commits", path: "", file: "" })}
          >
            Commits
          </button>
          <button
            className={`repo-tab${tab === "issues" ? " active" : ""}`}
            onClick={() => navigate({ tab: "issues", path: "", file: "" })}
          >
            Issues
          </button>
        </div>
      </div>

      {/* Empty repo message */}
      {branchInfo?.empty && (
        <div className="empty-state" style={{ marginTop: 32 }}>
          <h3>Empty repository</h3>
          <p>Push your first commit to get started.</p>
          {repo.cloneUrl && (
            <pre style={{ marginTop: 16, fontSize: 12, color: "var(--muted)", fontFamily: "var(--mono)", textAlign: "left", display: "inline-block" }}>
{`git clone ${repo.cloneUrl}
cd ${repo.name}
git commit --allow-empty -m "Initial commit"
git push origin main`}
            </pre>
          )}
        </div>
      )}

      {/* Code tab */}
      {!branchInfo?.empty && tab !== "commits" && (
        <>
          {/* Breadcrumb */}
          {displayedPath && (
            <div className="breadcrumb">
              <Link to={`/repos/${name}`} style={{ color: "var(--accent)", fontWeight: 600 }}>{name}</Link>
              {pathParts.map((part, i) => {
                const isLast = i === pathParts.length - 1;
                const partPath = pathParts.slice(0, i + 1).join("/");
                return (
                  <span key={i} style={{ display: "flex", alignItems: "center", gap: 4 }}>
                    <span className="breadcrumb-sep">/</span>
                    {isLast ? (
                      <span className="breadcrumb-current">{part}</span>
                    ) : (
                      <Link to={`/repos/${name}?ref=${encodeURIComponent(ref)}&path=${encodeURIComponent(partPath)}`} style={{ color: "var(--accent)", fontWeight: 600 }}>{part}</Link>
                    )}
                  </span>
                );
              })}
            </div>
          )}

          {/* File viewer */}
          {isViewingFile && blob && (
            <div className="file-viewer">
              <div className="file-viewer-header">
                <span className="file-viewer-name">{file.split("/").pop()}</span>
                {blob.encoding !== "binary" && blob.content !== null && (
                  <span style={{ color: "var(--faint)", fontSize: 12, marginLeft: "auto" }}>
                    {blob.content.split("\n").length} lines
                  </span>
                )}
              </div>
              {blob.encoding === "binary" ? (
                <div className="empty-state"><p>Binary file — cannot display</p></div>
              ) : (
                <div className="file-lines">
                  {(blob.content ?? "").split("\n").map((line, i) => (
                    <div key={i} className="file-line">
                      <span className="line-num">{i + 1}</span>
                      <span className="line-code">{line}</span>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}

          {/* File tree */}
          {!isViewingFile && tree !== null && (
            <div className="file-tree" style={{ marginTop: 16 }}>
              {path && (
                <div
                  className="file-tree-row"
                  onClick={() => {
                    const up = path.includes("/") ? path.substring(0, path.lastIndexOf("/")) : "";
                    navigate({ path: up, file: "" });
                  }}
                  style={{ color: "var(--muted)" }}
                >
                  <span className="file-tree-icon">…</span>
                  <span className="file-tree-name">..</span>
                </div>
              )}
              {tree.map((entry) => {
                const entryPath = path ? `${path}/${entry.name}` : entry.name;
                return (
                  <div
                    key={entry.name}
                    className="file-tree-row"
                    onClick={() => {
                      if (entry.type === "tree") navigate({ path: entryPath, file: "" });
                      else navigate({ file: entryPath, path: "" });
                    }}
                  >
                    <span className={`file-tree-icon${entry.type === "tree" ? " folder" : ""}`}>
                      {entry.type === "tree" ? <FolderIcon /> : <FileIcon />}
                    </span>
                    <span className="file-tree-name">{entry.name}</span>
                    {entry.type === "blob" && entry.size !== null && (
                      <span style={{ color: "var(--faint)", fontSize: 11, marginLeft: "auto" }}>
                        {entry.size < 1024 ? `${entry.size}B` : `${(entry.size / 1024).toFixed(1)}KB`}
                      </span>
                    )}
                  </div>
                );
              })}
              {tree.length === 0 && (
                <div className="empty-state"><p>Empty directory</p></div>
              )}
            </div>
          )}

          {/* README */}
          {!isViewingFile && !path && readme && (
            <div className="readme-panel">
              <div className="readme-header">
                <BookIcon /> README.md
              </div>
              <div className="readme-body">{readme}</div>
            </div>
          )}
        </>
      )}

      {/* Commits tab */}
      {tab === "commits" && (
        <div className="commit-list" style={{ marginTop: 16 }}>
          {commits === null && <p className="loading" style={{ padding: "16px 14px" }}>Loading commits…</p>}
          {commits !== null && commits.length === 0 && (
            <div className="empty-state"><p>No commits yet</p></div>
          )}
          {commits?.map((commit) => (
            <div key={commit.sha} className="commit-row">
              <span className="commit-sha">{commit.shortSha}</span>
              <div className="commit-body">
                <div className="commit-message">{commit.message}</div>
                <div className="commit-meta">
                  <span>{commit.author}</span>
                </div>
              </div>
              <span className="commit-time">{relativeTime(commit.date)}</span>
            </div>
          ))}
        </div>
      )}

      {/* Issues tab */}
      {tab === "issues" && (
        <div>
          <div className="issue-actions">
            <div className="issue-state-bar">
              <button
                className={`issue-state-btn${issueState === "open" ? " active" : ""}`}
                onClick={() => setIssueState("open")}
              >
                ● Open{issueData ? ` ${issueData.open}` : ""}
              </button>
              <button
                className={`issue-state-btn${issueState === "closed" ? " active" : ""}`}
                onClick={() => setIssueState("closed")}
              >
                ✓ Closed{issueData ? ` ${issueData.closed}` : ""}
              </button>
            </div>
            <button className="btn btn-primary btn-sm" onClick={() => setShowNewIssue(true)}>
              + New issue
            </button>
          </div>

          {issueData === null && <p className="loading" style={{ padding: "16px 0" }}>Loading…</p>}

          {issueData !== null && issueData.issues.length === 0 && (
            <div className="empty-state">
              <h3>No {issueState} issues</h3>
              <p>{issueState === "open" ? "Great — no open issues!" : "No closed issues yet."}</p>
            </div>
          )}

          {issueData !== null && issueData.issues.length > 0 && (
            <div className="issue-list">
              {issueData.issues.map((issue) => (
                <div key={issue.id} className="issue-row">
                  <span className={`issue-icon ${issue.state}`}>
                    {issue.state === "open" ? <IssueOpenIcon /> : <IssueClosedIcon />}
                  </span>
                  <div className="issue-body">
                    <div className="issue-title">
                      #{issue.number} {issue.title}
                      {issue.labels.map((l) => (
                        <span key={l} className="issue-label" style={{ marginLeft: 6 }}>{l}</span>
                      ))}
                    </div>
                    <div className="issue-meta">
                      <span>opened {relativeTime(issue.created_at)}</span>
                      {issue.author && <span>by {issue.author}</span>}
                      {issue.assignee && <span>→ {issue.assignee}</span>}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}

          {showNewIssue && name && (
            <NewIssueModal
              repoName={name}
              onClose={() => setShowNewIssue(false)}
              onCreated={() => {
                setShowNewIssue(false);
                setIssueData(null);
                fetch(`/api/repos/${name}/issues?state=${issueState}`)
                  .then((r) => r.json())
                  .then((d) => setIssueData(d));
              }}
            />
          )}
        </div>
      )}
    </div>
  );
}

function NewIssueModal({
  repoName,
  onClose,
  onCreated,
}: {
  repoName: string;
  onClose: () => void;
  onCreated: () => void;
}) {
  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [submitting, setSubmitting] = useState(false);
  const [err, setErr] = useState<string | null>(null);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    if (!title.trim()) return;
    setSubmitting(true);
    setErr(null);
    try {
      const res = await fetch(`/api/repos/${repoName}/issues`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ title: title.trim(), body }),
      });
      const data = await res.json() as { error?: string };
      if (!res.ok) { setErr(data.error ?? "Failed to create issue"); setSubmitting(false); return; }
      onCreated();
    } catch {
      setErr("Network error");
      setSubmitting(false);
    }
  }

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()} style={{ maxWidth: 560 }}>
        <h2 className="modal-title">New issue</h2>
        {err && <p className="error-msg" style={{ marginBottom: 16 }}>{err}</p>}
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label className="form-label">Title</label>
            <input
              className="form-input"
              placeholder="Short, descriptive title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              required
              autoFocus
            />
          </div>
          <div className="form-group">
            <label className="form-label">Description <span style={{ fontWeight: 400, color: "var(--faint)" }}>(optional)</span></label>
            <textarea
              className="form-input"
              placeholder="Describe the issue…"
              value={body}
              onChange={(e) => setBody(e.target.value)}
              rows={5}
              style={{ resize: "vertical", fontFamily: "inherit" }}
            />
          </div>
          <div className="form-actions">
            <button type="button" className="btn btn-secondary" onClick={onClose}>Cancel</button>
            <button type="submit" className="btn btn-primary" disabled={submitting || !title.trim()}>
              {submitting ? "Submitting…" : "Submit issue"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
