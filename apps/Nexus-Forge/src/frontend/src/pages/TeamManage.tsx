import { useState } from "react";

export default function TeamManage() {
  const [teamName, setTeamName] = useState("");

  return (
    <section>
      <h2>Team Management</h2>
      <p>Team and permission management is a stubbed experience in this prototype.</p>
      <form
        onSubmit={(event) => {
          event.preventDefault();
          setTeamName("");
        }}
      >
        <label>
          Team name
          <input value={teamName} onChange={(event) => setTeamName(event.target.value)} />
        </label>
        <button type="submit">Create team</button>
      </form>
      <div className="team-list">
        <p>No teams yet. Teams will appear here once created.</p>
      </div>
    </section>
  );
}
