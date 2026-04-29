import { useEffect, useState } from "react";

export default function LearningCenterHub() {
  const [tracks, setTracks] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/learning/tracks");
        const data = await res.json();
        setTracks((data.items || []).length);
      } catch {
        setTracks(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Learning Center Hub</h2>
      <p>Learning tracks, courses, certifications, and role-based capability paths.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Tracks</h3>
          <p>Tracks available: {tracks}</p>
        </article>
        <article className="repo-card">
          <h3>Courses</h3>
          <p>Course catalog and enrollment flows are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Certifications</h3>
          <p>Certification pathways are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
