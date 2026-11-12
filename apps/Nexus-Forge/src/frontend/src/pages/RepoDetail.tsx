import { useEffect, useState, useCallback } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";

interface RepoInfo { name: string; description?: string; visibility: string; cloneUrl?: string; }
interface BranchInfo { branches: string[]; default: string | null; empty: boolean; }
interface TreeEntry { name: string; type: "tree" | "blob"; size: number | null; }
interface Commit { sha: string; shortSha: string; message: string; author: string; date: string; }
interface BlobResult { content: string | null; encoding: string; path: string; }

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
  }, [name, branchInfo, loading, tab, rawRef, path, file]);

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
    </div>
  );
}
