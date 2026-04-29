import { useState } from "react";

export default function PullRequest() {
  const [title, setTitle] = useState("");
  const [description, setDescription] = useState("");

  return (
    <section>
      <h2>Pull Request Workspace</h2>
      <p>Draft PR UI is stubbed for the Nexus Forge roadmap.</p>
      <form onSubmit={(event) => event.preventDefault()}>
        <label>
          Title
          <input value={title} onChange={(event) => setTitle(event.target.value)} />
        </label>
        <label>
          Description
          <textarea value={description} onChange={(event) => setDescription(event.target.value)} />
        </label>
        <button type="submit">Create draft PR</button>
      </form>
    </section>
  );
}
