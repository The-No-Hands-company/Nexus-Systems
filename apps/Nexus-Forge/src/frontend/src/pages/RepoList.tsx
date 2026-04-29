import { useEffect, useState } from "react";
import { Link } from "react-router-dom";

interface Repository {
  id: number;
  name: string;
  description?: string;
  vcs: string;
  created_at: string;
}

export default function RepoList() {
  const [repos, setRepos] = useState<Repository[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    async function load() {
      const res = await fetch("/api/repos");
      const data = await res.json();
      setRepos(data.repos || []);
      setLoading(false);
    }
    load();
  }, []);

  return (
    <section>
      <h2>Repositories</h2>
      {loading ? (
        <p>Loading repositories…</p>
      ) : repos.length === 0 ? (
        <p>No repositories found. Create one to get started.</p>
      ) : (
        <div className="repo-grid">
          {repos.map((repo) => (
            <article key={repo.id} className="repo-card">
              <h3>{repo.name}</h3>
              <p>{repo.description || "No description available."}</p>
              <span className="badge">{repo.vcs}</span>
              <Link to={`/repos/${repo.name}`}>View repository</Link>
            </article>
          ))}
        </div>
      )}
    </section>
  );
}
