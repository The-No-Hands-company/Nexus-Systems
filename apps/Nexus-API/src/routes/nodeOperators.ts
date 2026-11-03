import { Router, type IRouter } from "express";

const router: IRouter = Router();

const HTML = /* html */ `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Node Operators — Nexus Systems</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    :root {
      --bg: #09090b;
      --surface: #18181b;
      --border: #27272a;
      --text: #e4e4e7;
      --muted: #71717a;
      --accent: #22d3ee;
      --accent-dim: #164e63;
      --green: #4ade80;
    }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      font-size: 16px;
      line-height: 1.6;
      padding: 0 16px;
    }
    a { color: var(--accent); text-decoration: none; }
    a:hover { text-decoration: underline; }
    .wrap { max-width: 760px; margin: 0 auto; padding: 56px 0 80px; }
    .wordmark { font-size: 14px; color: var(--muted); letter-spacing: 0.05em; text-transform: uppercase; margin-bottom: 48px; }
    h1 { font-size: clamp(28px, 5vw, 40px); font-weight: 700; letter-spacing: -0.02em; line-height: 1.15; }
    .lead { margin-top: 16px; font-size: 18px; color: var(--muted); max-width: 600px; }
    .cta-row { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 32px; }
    .btn { display: inline-block; padding: 10px 20px; border-radius: 8px; font-size: 15px; font-weight: 500; cursor: pointer; }
    .btn-primary { background: var(--accent); color: #000; }
    .btn-primary:hover { background: #06b6d4; text-decoration: none; }
    .btn-outline { border: 1px solid var(--border); color: var(--text); }
    .btn-outline:hover { background: var(--surface); text-decoration: none; }
    section { margin-top: 56px; }
    h2 { font-size: 20px; font-weight: 600; margin-bottom: 16px; padding-bottom: 10px; border-bottom: 1px solid var(--border); }
    h3 { font-size: 16px; font-weight: 600; margin: 24px 0 8px; }
    p { margin-bottom: 12px; }
    ul, ol { padding-left: 20px; margin-bottom: 12px; }
    li { margin-bottom: 6px; }
    table { width: 100%; border-collapse: collapse; font-size: 14px; margin-bottom: 12px; }
    th { text-align: left; color: var(--muted); font-weight: 500; padding: 8px 12px; border-bottom: 1px solid var(--border); }
    td { padding: 8px 12px; border-bottom: 1px solid var(--border); vertical-align: top; }
    tr:last-child td { border-bottom: none; }
    code { font-family: "SF Mono", "Fira Code", monospace; font-size: 13px; background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 1px 5px; }
    pre { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 16px; overflow-x: auto; font-size: 13px; line-height: 1.5; margin-bottom: 12px; }
    pre code { background: none; border: none; padding: 0; }
    .step { display: flex; gap: 16px; margin-bottom: 24px; }
    .step-num { flex-shrink: 0; width: 28px; height: 28px; border-radius: 50%; background: var(--accent-dim); color: var(--accent); font-size: 13px; font-weight: 700; display: flex; align-items: center; justify-content: center; margin-top: 2px; }
    .step-body { flex: 1; }
    .note { background: var(--surface); border: 1px solid var(--border); border-left: 3px solid var(--accent); border-radius: 6px; padding: 12px 16px; font-size: 14px; color: var(--muted); margin-bottom: 12px; }
    .tag { display: inline-block; font-size: 11px; font-weight: 600; padding: 2px 7px; border-radius: 4px; text-transform: uppercase; letter-spacing: 0.05em; margin-left: 8px; }
    .tag-public { background: var(--accent-dim); color: var(--accent); }
    .tag-auth { background: #1c1917; color: #a8a29e; border: 1px solid #292524; }
    footer { margin-top: 72px; padding-top: 24px; border-top: 1px solid var(--border); font-size: 14px; color: var(--muted); }
  </style>
</head>
<body>
<div class="wrap">
  <div class="wordmark">Nexus Systems</div>

  <h1>Become a Node Operator</h1>
  <p class="lead">
    Run a Nexus node on your own hardware and join the federated network.
    You keep full control — your machine, your keys, your data.
  </p>
  <div class="cta-row">
    <a class="btn btn-primary" href="https://app.tnhc.dev/node-operators">Enroll a node</a>
    <a class="btn btn-outline" href="https://github.com/The-No-Hands-Company/Nexus-Systems">View source on GitHub</a>
  </div>

  <section>
    <h2>What a node does</h2>
    <ul>
      <li>Hosts Nexus applications for users on that node</li>
      <li>Participates in federated routing — sites and mail can replicate across peers</li>
      <li>Holds an Ed25519 cryptographic identity that peers use to verify its messages</li>
      <li>Appears in the public node directory</li>
    </ul>
  </section>

  <section>
    <h2>What you need</h2>
    <table>
      <thead><tr><th>Requirement</th><th>Minimum</th><th>Notes</th></tr></thead>
      <tbody>
        <tr><td>Machine</td><td>Raspberry Pi 4 or small VPS</td><td>Must stay online to serve traffic</td></tr>
        <tr><td>Domain</td><td>Any resolvable hostname</td><td>e.g. <code>node.example.com</code> — used as the node's identity</td></tr>
        <tr><td>Storage</td><td>1 GB</td><td>Declared at enrollment; you set the number</td></tr>
        <tr><td>Bandwidth</td><td>1 GB / month</td><td>Declared at enrollment; you set the number</td></tr>
        <tr><td>Network</td><td>Outbound port 443</td><td>Inbound is optional — a reverse proxy or Cloudflare Tunnel works</td></tr>
        <tr><td>OS</td><td>Linux</td><td>The install script is a shell script; Bun is installed automatically</td></tr>
      </tbody>
    </table>
    <p class="note">You do not need a routable public IP. Any setup that terminates TLS for your domain is fine.</p>
  </section>

  <section>
    <h2>Enrollment</h2>
    <p>
      Enrollment is two steps. The split is intentional:
      this server never sees your private key — it is generated on your machine and stays there.
    </p>

    <h3>Step 1 — Register <span class="tag tag-auth">requires account</span></h3>
    <div class="step">
      <div class="step-num">1</div>
      <div class="step-body">
        Sign in at <a href="https://app.tnhc.dev">app.tnhc.dev</a>. No account yet?
        <a href="https://app.tnhc.dev/request">Request access</a>.
      </div>
    </div>
    <div class="step">
      <div class="step-num">2</div>
      <div class="step-body">
        Go to <a href="https://app.tnhc.dev/node-operators">app.tnhc.dev/node-operators</a>
        and click <strong>Enroll a node</strong>. Fill in your node's name, domain,
        region, and the storage and bandwidth you are willing to contribute.
      </div>
    </div>
    <div class="step">
      <div class="step-num">3</div>
      <div class="step-body">
        You receive a single-use <strong>enrollment token</strong> valid for 24 hours,
        and a ready-to-run install command. Copy both — the token is shown once and not stored.
      </div>
    </div>

    <h3>Step 2 — Claim <span class="tag tag-public">runs on your machine</span></h3>
    <div class="step">
      <div class="step-num">4</div>
      <div class="step-body">
        On the machine that will be the node, run the install command you received:
        <pre><code>curl -fsSL https://hosting.tnhc.dev/install-node.sh -o install-node.sh \\
  &amp;&amp; sh install-node.sh --token &lt;your-token&gt; --api https://hosting.tnhc.dev</code></pre>
        The script generates an Ed25519 keypair locally, signs the token with the private key,
        and registers the public key with the network.
      </div>
    </div>
    <div class="step">
      <div class="step-num">5</div>
      <div class="step-body">
        Your node moves from <code>pending</code> → <code>active</code>.
        Peers can now discover and verify it by its registered public key.
      </div>
    </div>

    <p class="note">
      Token expired? Ask an operator to revoke it and start a fresh enrollment.
      Tokens expire after 24 hours and cannot be extended.
    </p>
  </section>

  <section>
    <h2>API reference</h2>
    <table>
      <thead><tr><th>Endpoint</th><th>Auth</th><th>Description</th></tr></thead>
      <tbody>
        <tr><td><code>GET /nodes</code></td><td><span class="tag tag-public">Public</span></td><td>Federation directory — lists all active nodes</td></tr>
        <tr><td><code>POST /nodes/enroll</code></td><td><span class="tag tag-auth">Session</span></td><td>Start enrollment; returns single-use token</td></tr>
        <tr><td><code>POST /nodes/claim</code></td><td><span class="tag tag-auth">Token</span></td><td>Finish enrollment with public key + signature</td></tr>
        <tr><td><code>POST /nodes/:id/enrollment/revoke</code></td><td><span class="tag tag-auth">Session</span></td><td>Revoke an unclaimed token</td></tr>
      </tbody>
    </table>
    <p>Base URL: <code>https://hosting.tnhc.dev</code></p>
  </section>

  <footer>
    <p>
      Questions? Open an issue on
      <a href="https://github.com/The-No-Hands-Company/Nexus-Systems">GitHub</a>
      or reach out directly.
      Running even a temporary node helps stress-test the federation protocol — it is appreciated.
    </p>
    <p style="margin-top:8px">
      <a href="https://app.tnhc.dev">app.tnhc.dev</a> &nbsp;·&nbsp;
      <a href="https://tnhc.dev">tnhc.dev</a>
    </p>
  </footer>
</div>
</body>
</html>`;

router.get("/node-operators", (_req, res) => {
  res.setHeader("content-type", "text/html; charset=utf-8");
  res.setHeader("cache-control", "public, max-age=300");
  res.end(HTML);
});

export default router;
