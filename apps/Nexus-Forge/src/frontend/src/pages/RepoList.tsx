import { useEffect, useState } from "react";
import { Link } from "react-router-dom";

interface Repository {
  name: string;
  description?: string;
  visibility: string;
  created_at: string;
}

function relativeTime(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  const mins = Math.floor(diff / 60000);
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  const days = Math.floor(hrs / 24);
  if (days < 30) return `${days}d ago`;
  return new Date(iso).toLocaleDateString();
}

function LockIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4 7V5a4 4 0 1 1 8 0v2h1a1 1 0 0 1 1 1v6a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V8a1 1 0 0 1 1-1h1zm2 0h4V5a2 2 0 1 0-4 0v2z"/>
    </svg>
  );
}

function GlobeIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">
      <path d="M8 1a7 7 0 1 0 0 14A7 7 0 0 0 8 1zM2.3 8.7A5.98 5.98 0 0 0 8 14a5.98 5.98 0 0 0 5.7-5.3H2.3zM13.7 7.3A5.98 5.98 0 0 0 8 2a5.98 5.98 0 0 0-5.7 5.3h11.4z"/>
    </svg>
  );
}

export default function RepoList() {
  const [repos, setRepos] = useState<Repository[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [showCreate, setShowCreate] = useState(false);

  useEffect(() => {
    fetch("/api/repos")
      .then((r) => r.json())
      .then((d) => { setRepos(d.repos ?? []); setLoading(false); })
      .catch(() => { setError("Failed to load repositories"); setLoading(false); });
  }, []);

  return (
    <div>
      <div className="page-header">
        <h1 className="page-title">Repositories</h1>
        <button className="btn btn-primary btn-sm" onClick={() => setShowCreate(true)}>
          + New repository
        </button>
      </div>

      {loading && <p className="loading">Loading…</p>}
      {error && <p className="error-msg">{error}</p>}

      {!loading && !error && repos.length === 0 && (
        <div className="empty-state">
          <h3>No repositories yet</h3>
          <p>Create your first repository to get started.</p>
        </div>
      )}

      {repos.length > 0 && (
        <div className="repo-list">
          {repos.map((repo) => (
            <div key={repo.name} className="repo-row">
              <span className="repo-row-icon">
                {repo.visibility === "public" ? <GlobeIcon /> : <LockIcon />}
              </span>
              <div className="repo-row-body">
                <Link to={`/repos/${repo.name}`} className="repo-row-name">
                  {repo.name}
                  <span className={`badge badge-${repo.visibility}`}>
                    {repo.visibility}
                  </span>
                </Link>
                {repo.description && (
                  <div className="repo-row-desc">{repo.description}</div>
                )}
              </div>
              <span className="repo-row-time">{relativeTime(repo.created_at)}</span>
            </div>
          ))}
        </div>
      )}

      {showCreate && (
        <CreateRepoModal
          onClose={() => setShowCreate(false)}
          onCreated={(name) => {
            setShowCreate(false);
            window.location.href = `/repos/${name}`;
          }}
        />
      )}
    </div>
  );
}

function CreateRepoModal({ onClose, onCreated }: { onClose: () => void; onCreated: (name: string) => void }) {
  const [name, setName] = useState("");
  const [desc, setDesc] = useState("");
  const [visibility, setVisibility] = useState<"private" | "public">("private");
  const [submitting, setSubmitting] = useState(false);
  const [err, setErr] = useState<string | null>(null);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setSubmitting(true);
    setErr(null);
    try {
      const res = await fetch("/api/repos", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: name.trim(), description: desc.trim(), visibility }),
      });
      const data = await res.json() as { repo?: { name: string }; error?: string };
      if (!res.ok) { setErr(data.error ?? "Failed to create"); setSubmitting(false); return; }
      onCreated(data.repo?.name ?? name);
    } catch {
      setErr("Network error");
      setSubmitting(false);
    }
  }

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <h2 className="modal-title">New repository</h2>
        {err && <p className="error-msg" style={{ marginBottom: 16 }}>{err}</p>}
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label className="form-label">Name</label>
            <input
              className="form-input"
              placeholder="my-project"
              value={name}
              onChange={(e) => setName(e.target.value)}
              required
              autoFocus
            />
          </div>
          <div className="form-group">
            <label className="form-label">Description <span style={{ fontWeight: 400, color: "var(--faint)" }}>(optional)</span></label>
            <input
              className="form-input"
              placeholder="A short description…"
              value={desc}
              onChange={(e) => setDesc(e.target.value)}
            />
          </div>
          <div className="form-group">
            <label className="form-label">Visibility</label>
            <select className="form-input" value={visibility} onChange={(e) => setVisibility(e.target.value as "private" | "public")}>
              <option value="private">Private</option>
              <option value="public">Public</option>
            </select>
          </div>
          <div className="form-actions">
            <button type="button" className="btn btn-secondary" onClick={onClose}>Cancel</button>
            <button type="submit" className="btn btn-primary" disabled={submitting}>
              {submitting ? "Creating…" : "Create repository"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
