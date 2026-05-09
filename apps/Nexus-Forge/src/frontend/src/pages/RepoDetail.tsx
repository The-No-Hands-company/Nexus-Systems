import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";

interface RepoDetailData {
  name?: string;
  description?: string;
  vcs?: string;
  cloneUrl?: string;
}

interface ActivityItem {
  id?: string;
  action?: string;
  created_at?: string;
}

export default function RepoDetail() {
  const params = useParams();
  const [repo, setRepo] = useState<RepoDetailData | null>(null);
  const [activity, setActivity] = useState<ActivityItem[]>([]);

  useEffect(() => {
    async function load() {
      const name = params.name;
      if (!name) return;
      const res = await fetch(`/api/repos/${name}`);
      const data = await res.json();
      setRepo(data);
      const activityRes = await fetch(`/api/repos/${name}/activity`);
      const activityData = await activityRes.json();
      setActivity(activityData.activity || []);
    }
    load();
  }, [params.name]);

  if (!repo) {
    return <p>Loading repository details…</p>;
  }

  return (
    <section>
      <Link to="/">← Back to repositories</Link>
      <h2>{repo.name ?? "Unnamed repository"}</h2>
      <p>{repo.description ?? "Repository details are stubbed."}</p>
      <div className="repo-meta">
        <p>VCS: {(repo.vcs as string) || "git"}</p>
        <p>Clone URL: {(repo.cloneUrl as string) || "N/A"}</p>
      </div>
      <div className="activity-panel">
        <h3>Activity</h3>
        {activity.length === 0 ? (
          <p>No activity yet.</p>
        ) : (
          <ul>
            {activity.map((item) => (
              <li
                key={`${item.id ?? item.action ?? "activity-event"}-${item.created_at ?? "unknown"}`}
              >
                {item.action || "activity event"}
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}
