import { useEffect, useState } from "react";

export default function FederatedIdentityAssuranceHub() {
  const [protocols, setProtocols] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/federated-identity/protocols");
        const data = await res.json();
        setProtocols(data.entity?.protocols || []);
      } catch {
        setProtocols([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Federated Identity Assurance Hub</h2>
      <p>Provider trust management, assertion verification, and protocol assurance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Protocols</h3>
          <p>{protocols.length > 0 ? protocols.join(", ") : "No protocols loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Providers</h3>
          <p>Provider catalog APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Trust Policies</h3>
          <p>Trust policy workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
