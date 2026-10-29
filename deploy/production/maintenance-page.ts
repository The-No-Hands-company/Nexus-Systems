// Shared maintenance page for the Nexus platform edge.
//
// Rendered by the proxy when Nexus-Cloud reports a route's part is under
// operator-set maintenance. The page must stand entirely on its own — the proxy
// serves it as a 503 body with no app behind it — so everything is inline and
// there are no external requests.
//
// The contract the page exists to honour: a visitor must immediately see (1)
// which part is down, (2) that it is deliberate maintenance rather than a
// failure, and (3) when it is expected back. The operator provides those
// facts; this only presents them.

export type MaintenanceInfo = {
  /** Human name of the part that is down, e.g. "Nexus Hosting" or "Chat". */
  part: string;
  /** Operator's note on why, shown verbatim. Optional. */
  reason?: string;
  /** ISO timestamp maintenance began. Optional, shown as "since". */
  startedAt?: string;
  /** ISO timestamp it is expected back. Drives the "expected back" line and the
   *  live countdown. Omitted means the duration is not yet known. */
  expectedEndAt?: string;
  /** Status/support contact shown in the footer, e.g. "status.tnhc.dev". */
  statusUrl?: string;
};

function esc(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

/** Validates and returns an ISO instant, or null — never trusts caller input
 *  enough to put a malformed value into the inline script. */
function isoOrNull(value: string | undefined): string | null {
  if (!value) return null;
  const ms = Date.parse(value);
  return Number.isNaN(ms) ? null : new Date(ms).toISOString();
}

function humanWhen(iso: string): string {
  // A fixed, unambiguous UTC rendering as the no-JS fallback; the inline script
  // upgrades it to the viewer's locale and keeps the countdown live.
  const d = new Date(iso);
  return `${d.toUTCString()}`;
}

/** Full HTML document for the maintenance page. */
export function renderMaintenancePage(info: MaintenanceInfo): string {
  const part = esc(info.part || "This service");
  const reason = info.reason ? esc(info.reason) : "";
  const startedAt = isoOrNull(info.startedAt);
  const expectedEndAt = isoOrNull(info.expectedEndAt);
  const statusUrl = info.statusUrl ? esc(info.statusUrl) : "";

  const expectedLine = expectedEndAt
    ? `<p class="when" id="when">Expected back by <time datetime="${expectedEndAt}">${humanWhen(expectedEndAt)}</time></p>
       <p class="countdown" id="countdown" aria-live="polite"></p>`
    : `<p class="when">We don't have an estimated time yet — please check back shortly.</p>`;

  const sinceLine = startedAt
    ? `<p class="since">In maintenance since <time datetime="${startedAt}">${humanWhen(startedAt)}</time></p>`
    : "";

  const reasonLine = reason ? `<p class="reason">${reason}</p>` : "";

  const footer = statusUrl
    ? `<footer>Live status: <a href="https://${statusUrl}">${statusUrl}</a></footer>`
    : `<footer>Nexus</footer>`;

  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>${part} — under maintenance</title>
<meta name="robots" content="noindex" />
<style>
  :root {
    color-scheme: light dark;
    --bg: #f6f7f9; --card: #ffffff; --ink: #18181b; --muted: #52525b;
    --line: #e4e4e7; --accent: #b45309; --accent-bg: #fef3c7; --accent-ink: #92400e;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg: #09090b; --card: #18181b; --ink: #fafafa; --muted: #a1a1aa;
      --line: #27272a; --accent: #fbbf24; --accent-bg: #27210a; --accent-ink: #fcd34d;
    }
  }
  * { box-sizing: border-box; }
  html, body { height: 100%; }
  body {
    margin: 0; background: var(--bg); color: var(--ink);
    font: 16px/1.6 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    display: grid; place-items: center; padding: 24px;
  }
  main {
    width: 100%; max-width: 32rem; background: var(--card);
    border: 1px solid var(--line); border-radius: 16px; padding: 2.25rem;
    box-shadow: 0 1px 2px rgba(0,0,0,.04), 0 8px 24px rgba(0,0,0,.06);
  }
  .badge {
    display: inline-flex; align-items: center; gap: .5rem;
    background: var(--accent-bg); color: var(--accent-ink);
    font-size: .8125rem; font-weight: 600; letter-spacing: .02em;
    padding: .375rem .75rem; border-radius: 999px; text-transform: uppercase;
  }
  .dot {
    width: .5rem; height: .5rem; border-radius: 999px; background: var(--accent);
    animation: pulse 2s ease-in-out infinite;
  }
  @keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: .35; } }
  @media (prefers-reduced-motion: reduce) { .dot { animation: none; } }
  h1 { margin: 1.25rem 0 .5rem; font-size: 1.5rem; line-height: 1.25; }
  h1 .part { color: var(--accent-ink); }
  p { margin: .5rem 0; color: var(--muted); }
  .reason {
    margin-top: 1rem; padding: .75rem 1rem; background: var(--bg);
    border: 1px solid var(--line); border-radius: 10px; color: var(--ink);
  }
  .when { margin-top: 1.25rem; color: var(--ink); font-weight: 500; }
  .countdown { font-variant-numeric: tabular-nums; color: var(--muted); }
  .since { font-size: .875rem; }
  footer {
    margin-top: 1.75rem; padding-top: 1rem; border-top: 1px solid var(--line);
    font-size: .8125rem; color: var(--muted);
  }
  footer a { color: var(--accent); }
</style>
</head>
<body>
<main role="main">
  <span class="badge"><span class="dot"></span> Scheduled maintenance</span>
  <h1><span class="part">${part}</span> is temporarily down for maintenance</h1>
  <p>We're making planned improvements. Nothing is broken — this part is offline on purpose and will be back.</p>
  ${reasonLine}
  ${expectedLine}
  ${sinceLine}
  ${footer}
</main>
<script>
(function () {
  "use strict";
  // Upgrade the UTC fallbacks to the viewer's locale.
  try {
    document.querySelectorAll("time[datetime]").forEach(function (el) {
      var d = new Date(el.getAttribute("datetime"));
      if (!isNaN(d)) el.textContent = d.toLocaleString();
    });
  } catch (e) {}

  var end = ${expectedEndAt ? `new Date(${JSON.stringify(expectedEndAt)})` : "null"};
  var out = document.getElementById("countdown");
  if (end && out && !isNaN(end)) {
    var tick = function () {
      var ms = end.getTime() - Date.now();
      if (ms <= 0) {
        out.textContent = "Maintenance should be complete — try reloading.";
        clearInterval(timer);
        return;
      }
      var s = Math.floor(ms / 1000);
      var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
      var parts = [];
      if (h) parts.push(h + "h");
      parts.push(m + "m");
      parts.push(sec + "s");
      out.textContent = "About " + parts.join(" ") + " remaining";
    };
    tick();
    var timer = setInterval(tick, 1000);
  }

  // Reload automatically a little after the expected end so a waiting visitor
  // lands back on the live app without having to refresh by hand.
  if (end && !isNaN(end)) {
    var delay = end.getTime() - Date.now() + 5000;
    if (delay > 0 && delay < 86400000) setTimeout(function () { location.reload(); }, delay);
  }
})();
</script>
</body>
</html>`;
}
