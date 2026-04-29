import { useEffect, useState } from "react";

export default function CustomerDataPrivacyVaultHub() {
  const [regulations, setRegulations] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/privacy-vault/compliance");
        const data = await res.json();
        setRegulations(data.entity?.regulations || []);
      } catch {
        setRegulations([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Customer Data Privacy Vault Hub</h2>
      <p>Consent management, data access requests, and privacy regulation compliance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Consents</h3>
          <p>Customer consents are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Requests</h3>
          <p>Privacy requests are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Compliance</h3>
          <p>{regulations.join(", ") || "No regulations"}</p>
        </article>
      </div>
    </section>
  );
}
