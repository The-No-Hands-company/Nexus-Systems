import { useEffect, useState } from "react";

export default function NotificationOrchestrationHub() {
  const [delivery, setDelivery] = useState<{ delivered: number; failed: number }>({
    delivered: 0,
    failed: 0,
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/notifications/delivery");
        const data = await res.json();
        setDelivery(data.entity || { delivered: 0, failed: 0 });
      } catch {
        setDelivery({ delivered: 0, failed: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Notification Orchestration Hub</h2>
      <p>Multi-channel notifications, templates, and scheduled delivery orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Channels</h3>
          <p>Notification channels are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Templates</h3>
          <p>Notification templates are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Delivery</h3>
          <p>
            Delivered: {delivery.delivered}, Failed: {delivery.failed}
          </p>
        </article>
      </div>
    </section>
  );
}
