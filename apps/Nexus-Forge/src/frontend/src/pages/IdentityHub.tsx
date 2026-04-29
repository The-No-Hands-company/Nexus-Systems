import { useEffect, useState } from "react";

export default function IdentityHub() {
  const [users, setUsers] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/identity/users");
        const data = await res.json();
        setUsers((data.items || []).length);
      } catch {
        setUsers(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Identity Hub</h2>
      <p>Users, groups, roles, sessions, and identity audit controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Users</h3>
          <p>Provisioned users: {users}</p>
        </article>
        <article className="repo-card">
          <h3>Groups and Roles</h3>
          <p>Role and group lifecycle endpoints are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Session Security</h3>
          <p>Session management and identity audit are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
