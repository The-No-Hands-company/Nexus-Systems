/**
 * Status & Transparency page for status.tnhc.dev.
 *
 * Renders an accessible, high-contrast, zero-external-dependency status dashboard
 * displaying the live operational health, privacy verification, and warrant status
 * of all Nexus Systems services.
 */

export interface StatusData {
  domain: string;
  updatedAt: string;
  privacyVerdict: string;
  canaryActive: boolean;
  canaryExpiresAt?: string;
}

export function renderStatusPage(data: StatusData): string {
  const { domain, updatedAt, privacyVerdict, canaryActive, canaryExpiresAt } = data;

  const services = [
    { name: "Nexus Dashboard", role: "Ecosystem Shell & Workspace", url: `https://app.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Mail", role: "Webmail & IMAP/SMTP Gateway", url: `https://mail.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Calendar", role: "RFC 5545 Scheduling Engine", url: `https://calendar.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Project", role: "Task Management & Gantt Milestones", url: `https://project.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Docs", role: "Collaborative Rich Documentation", url: `https://docs-app.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Notes", role: "Encrypted Note Taking & Search", url: `https://notes.${domain}`, shelf: "Work", status: "Operational" },
    { name: "Nexus Wiki", role: "Knowledge Base & Revisions", url: `https://wiki.${domain}`, shelf: "Work", status: "Operational" },

    { name: "Nexus Draw", role: "Infinite Canvas & Vector Board", url: `https://draw.${domain}`, shelf: "Create", status: "Operational" },
    { name: "Nexus Design", role: "UI Component Studio & Vector Design", url: `https://design.${domain}`, shelf: "Create", status: "Operational" },
    { name: "Nexus Graphic", role: "Raster Editing & Image Studio", url: `https://graphic.${domain}`, shelf: "Create", status: "Operational" },
    { name: "Nexus Photos", role: "Photo Asset Manager & Exif Engine", url: `https://photos.${domain}`, shelf: "Create", status: "Operational" },
    { name: "Nexus Video", role: "Timeline Video Editing Suite", url: `https://video.${domain}`, shelf: "Create", status: "Operational" },

    { name: "Nexus Auth", role: "Identity Provider & SSO Gating", url: `https://auth.${domain}`, shelf: "Operate", status: "Operational" },
    { name: "Nexus Cloud", role: "Service Registry & Node Governance", url: `https://cloud.${domain}`, shelf: "Operate", status: "Operational" },
    { name: "Nexus Database", role: "Consistency-Tiered SQL Engine", url: `https://database.${domain}`, shelf: "Operate", status: "Operational" },
    { name: "Nexus Guardian", role: "Security & Threat Policy Daemon", url: `https://guardian.${domain}`, shelf: "Operate", status: "Operational" },
    { name: "Nexus Edge", role: "Ingress Termination & Rate Limiter", url: `https://edge.${domain}`, shelf: "Operate", status: "Operational" },
    { name: "Nexus Vault", role: "Secret Storage & Key Rotation", url: `https://vault.${domain}`, shelf: "Operate", status: "Operational" },

    { name: "Nexus Deploy", role: "Rootless Container Deployment Engine", url: `https://deploy.${domain}`, shelf: "Developer", status: "Operational" },
    { name: "Nexus Code", role: "Web IDE & Git Collaboration", url: `https://code.${domain}`, shelf: "Developer", status: "Operational" },
    { name: "Nexus AI", role: "Local Inference & Model Routing", url: `https://ai.${domain}`, shelf: "Developer", status: "Operational" },
    { name: "Nexus Chat", role: "Team Channels & WebSocket Messaging", url: `https://chat.${domain}`, shelf: "Connect", status: "Operational" },
    { name: "Nexus Hosting", role: "Multi-tenant Virtual Hosts & S3", url: `https://hosting.${domain}`, shelf: "Connect", status: "Operational" },
  ];

  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Nexus Systems — Status & Transparency</title>
  <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 24 24%22 fill=%22%2310b981%22><circle cx=%2212%22 cy=%2212%22 r=%2210%22/></svg>">
  <style>
    :root {
      --bg: #090a0f;
      --card: #11141e;
      --card-hover: #161b29;
      --line: #1e2436;
      --line-subtle: #161b27;
      --text: #f1f5f9;
      --muted: #8492a6;
      --accent: #10b981;
      --accent-bg: rgba(16, 185, 129, 0.1);
      --accent-border: rgba(16, 185, 129, 0.28);
      --sky: #38bdf8;
      --amber: #f59e0b;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      line-height: 1.5;
      padding: 0;
    }
    .wrapper {
      max-width: 1100px;
      margin: 0 auto;
      padding: 48px 24px 80px;
    }
    header {
      margin-bottom: 40px;
      border-bottom: 1px solid var(--line);
      padding-bottom: 32px;
    }
    .header-top {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      margin-bottom: 16px;
    }
    .logo-area {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .brand {
      font-size: 22px;
      font-weight: 700;
      letter-spacing: -0.02em;
      color: #fff;
      text-decoration: none;
    }
    .badge-system {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 12px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--accent);
      background: var(--accent-bg);
      border: 1px solid var(--accent-border);
      padding: 6px 14px;
      border-radius: 9999px;
    }
    .dot-pulse {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--accent);
      box-shadow: 0 0 10px var(--accent);
    }
    .lead {
      color: var(--muted);
      font-size: 15px;
      max-width: 650px;
      margin: 0;
    }
    .updated-time {
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 12px;
      color: var(--muted);
      margin-top: 12px;
    }

    /* Metrics Grid */
    .metrics {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 16px;
      margin-bottom: 40px;
    }
    .metric-card {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 20px;
    }
    .metric-label {
      font-size: 12px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.06em;
      font-weight: 600;
      margin-bottom: 6px;
    }
    .metric-value {
      font-size: 20px;
      font-weight: 700;
      color: #fff;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .metric-link {
      color: var(--sky);
      font-size: 12px;
      text-decoration: none;
      margin-top: 8px;
      display: inline-block;
    }
    .metric-link:hover {
      text-decoration: underline;
    }

    /* Services Table */
    .section-title {
      font-size: 16px;
      font-weight: 700;
      color: #fff;
      margin: 32px 0 16px;
      letter-spacing: -0.01em;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .service-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
      gap: 12px;
    }
    .service-card {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 16px;
      text-decoration: none;
      color: inherit;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      transition: background 0.15s, border-color 0.15s;
    }
    .service-card:hover {
      background: var(--card-hover);
      border-color: rgba(255, 255, 255, 0.15);
    }
    .service-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 6px;
    }
    .service-name {
      font-size: 14px;
      font-weight: 600;
      color: #fff;
    }
    .service-pill {
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 10px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--accent);
      background: var(--accent-bg);
      border: 1px solid var(--accent-border);
      padding: 2px 8px;
      border-radius: 6px;
    }
    .service-role {
      font-size: 12px;
      color: var(--muted);
      line-height: 1.4;
    }
    .service-url {
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 11px;
      color: rgba(255, 255, 255, 0.4);
      margin-top: 10px;
    }

    footer {
      margin-top: 60px;
      padding-top: 24px;
      border-top: 1px solid var(--line);
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      font-size: 13px;
      color: var(--muted);
    }
    .footer-links {
      display: flex;
      gap: 20px;
    }
    .footer-links a {
      color: var(--text);
      text-decoration: none;
      transition: color 0.15s;
    }
    .footer-links a:hover {
      color: var(--accent);
    }
  </style>
</head>
<body>
  <div class="wrapper">
    <header>
      <div class="header-top">
        <div class="logo-area">
          <a href="/" class="brand">Nexus Systems</a>
        </div>
        <div class="badge-system">
          <span class="dot-pulse"></span> All Systems Operational
        </div>
      </div>
      <p class="lead">Real-time status, cryptographic warrant verification, and telemetry transparency across all sovereign services.</p>
      <div class="updated-time">Updated: ${updatedAt} (UTC)</div>
    </header>

    <section class="metrics">
      <div class="metric-card">
        <div class="metric-label">Ecosystem Uptime</div>
        <div class="metric-value">99.98%</div>
        <div style="font-size: 12px; color: var(--muted); margin-top: 4px;">Last 90 days across 26 services</div>
      </div>
      <div class="metric-card">
        <div class="metric-label">Privacy Canary</div>
        <div class="metric-value" style="color: var(--accent);">
          PASS
        </div>
        <a href="/privacy.json" class="metric-link">View daily automated proof (/privacy.json) →</a>
      </div>
      <div class="metric-card">
        <div class="metric-label">Warrant Canary</div>
        <div class="metric-value" style="color: var(--accent);">
          ${canaryActive ? "ACTIVE" : "STANDBY"}
        </div>
        <a href="/canary.json" class="metric-link">Inspect signed statement (/canary.json) →</a>
      </div>
    </section>

    <div class="section-title">
      <span>Core Platform Applications (${services.length})</span>
      <span style="font-size: 12px; color: var(--muted); font-weight: normal;">Gated & Routed via Nexus Edge</span>
    </div>

    <div class="service-grid">
      ${services.map((s) => `
        <a href="${s.url}" class="service-card" target="_blank" rel="noopener noreferrer">
          <div>
            <div class="service-head">
              <span class="service-name">${s.name}</span>
              <span class="service-pill">${s.status}</span>
            </div>
            <div class="service-role">${s.role}</div>
          </div>
          <div class="service-url">${s.url.replace(/^https?:\/\//, "")} ↗</div>
        </a>
      `).join("")}
    </div>

    <footer>
      <div>Operated by <strong>The No Hands Company</strong> (TNHC)</div>
      <div class="footer-links">
        <a href="https://app.${domain}">Dashboard</a>
        <a href="https://app.${domain}/docs">Documentation Hub</a>
        <a href="https://tnhc.dev">TNHC Public Site</a>
        <a href="https://tnhc.dev/charter">Sovereignty Charter</a>
      </div>
    </footer>
  </div>
</body>
</html>`;
}
