/**
 * The apex sign-in page.
 *
 * The ecosystem model is one login at tnhc.dev that reaches every app on a
 * subdomain. Until now each app had to present its own form, which is both the
 * thing users complained about and the reason parallel identity systems kept
 * appearing. This is the single place a human types a password.
 *
 * A server-rendered form rather than a JSON call from a page: the browser then
 * receives Set-Cookie on a top-level navigation and follows a 303 back to
 * wherever the user was going, with no client-side token handling at all.
 */

/**
 * Decide whether we are willing to send the browser to `target` after login.
 *
 * An unvalidated redirect parameter is an open redirect: an attacker sends
 * someone a link to the real, correctly-certificated login page with
 * ?redirect=https://evil.example, the user signs in for real, and lands on a
 * page of the attacker's choosing carrying the trust of having just
 * authenticated. Only same-site destinations are allowed.
 *
 * `cookieDomain` is the parent domain the session cookie is scoped to
 * (".tnhc.dev"); anything at or under it is a legitimate destination. With no
 * cookie domain configured — the local default — only relative paths pass.
 */
export function safeRedirect(target: string | null, cookieDomain: string | null): string | null {
  if (!target) return null;

  // Relative paths are always fine, except protocol-relative "//evil.example",
  // which a browser treats as absolute.
  if (target.startsWith("/") && !target.startsWith("//")) return target;

  let url: URL;
  try {
    url = new URL(target);
  } catch {
    return null;
  }
  if (url.protocol !== "https:" && url.protocol !== "http:") return null;
  if (!cookieDomain) return null;

  const parent = cookieDomain.startsWith(".") ? cookieDomain.slice(1) : cookieDomain;
  const host = url.hostname.toLowerCase();
  return host === parent || host.endsWith(`.${parent}`) ? url.toString() : null;
}

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

export function renderLoginPage(options: {
  redirect: string | null;
  error?: string;
  appName?: string;
}): string {
  const redirectField = options.redirect
    ? `<input type="hidden" name="redirect" value="${escapeHtml(options.redirect)}" />`
    : "";
  const destination = options.redirect
    ? `<p class="dest">You'll be returned to <code>${escapeHtml(options.redirect)}</code></p>`
    : "";

  const errorId = options.error ? "login-err" : null;
  const formDescBy = errorId ? ` aria-describedby="${errorId}"` : "";

  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
<title>Sign in — Nexus Systems</title>
<style>
  :root {
    --bg: #0b0d12; --surface: #141821; --fg: #e8eaf0; --muted: #8b93a7;
    --subtle: #b6bdcd; --dim: #8490a8; --border: #2b3145; --border-focus: #232838;
    --input-bg: #0e1119; --accent: #4c7dff; --accent-hover: #3d6bec;
    --err-bg: #3a1720; --err-border: #6b2333; --err-fg: #ffb4c0;
    --link: #8fb0ff; --brand: #ccff00;
    color-scheme: dark;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100dvh; display: grid; place-items: center;
    font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
    background: var(--bg); color: var(--fg);
    padding: max(24px, env(safe-area-inset-top)) 16px max(24px, env(safe-area-inset-bottom));
  }
  .card {
    width: min(100%, 380px); padding: clamp(20px, 6vw, 32px);
    background: var(--surface); border: 1px solid var(--border-focus); border-radius: 14px;
  }
  .brand-mark { display: flex; align-items: center; gap: 8px; margin-bottom: 18px; }
  .brand-mark svg { color: var(--brand); flex-shrink: 0; }
  .brand-mark span { font-size: .8rem; font-weight: 600; letter-spacing: .08em; text-transform: uppercase; color: var(--subtle); }
  h1 { margin: 0 0 4px; font-size: 1.3rem; text-wrap: balance; }
  .sub { margin: 0 0 22px; color: var(--muted); font-size: .88rem; text-wrap: balance; }
  label { display: block; margin: 14px 0 6px; font-size: .82rem; color: var(--subtle); letter-spacing: .01em; }
  input[type=text], input[type=password] {
    width: 100%; padding: 10px 12px; border-radius: 8px;
    border: 1px solid var(--border); background: var(--input-bg); color: inherit; font: inherit;
    transition: border-color .15s;
  }
  input:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; border-color: var(--accent); }
  button {
    width: 100%; margin-top: 20px; padding: 11px; border: 0; border-radius: 8px;
    background: var(--accent); color: #fff; font: 600 15px/1 inherit; cursor: pointer;
    transition: background .15s; min-height: 44px;
  }
  button:hover { background: var(--accent-hover); }
  button:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
  .err {
    margin: 14px 0 0; padding: 9px 11px; border-radius: 8px;
    background: var(--err-bg); border: 1px solid var(--err-border); color: var(--err-fg); font-size: .85rem;
  }
  .dest { margin: 16px 0 0; font-size: .76rem; color: var(--dim); word-break: break-all; }
  code { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
  .divider { border: 0; border-top: 1px solid var(--border); margin: 20px 0 0; }
  nav.ways-in { margin: 12px 0 0; font-size: .78rem; color: var(--muted); display: flex; flex-direction: column; gap: 4px; }
  nav.ways-in p { margin: 0; }
  nav.ways-in a { color: var(--link); text-underline-offset: 2px; }
  nav.ways-in a:hover { text-decoration-thickness: 2px; }
  nav.ways-in a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; border-radius: 2px; }
</style>
</head>
<body>
  <main class="card">
    <div class="brand-mark" aria-hidden="true">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
      <span>Nexus Systems</span>
    </div>
    <h1>Sign in</h1>
    <p class="sub">One account for every Nexus app.</p>
    <form method="post" action="/login"${formDescBy}>
      ${redirectField}
      <label for="username">Username</label>
      <input id="username" name="username" type="text" autocomplete="username" autofocus required />
      <label for="password">Password</label>
      <input id="password" name="password" type="password" autocomplete="current-password" required />
      <button type="submit">Sign in</button>
      ${options.error ? `<p class="err" role="alert" id="login-err">${escapeHtml(options.error)}</p>` : ""}
    </form>
    ${destination}
    <hr class="divider" />
    <nav class="ways-in" aria-label="Other sign-in options">
      <p>New here? <a href="/request">Request access</a> · <a href="/join">Join with an invite</a> · <a href="/claim">Claim an approved account</a></p>
      <p>Lost your password? <a href="/recover">Use a recovery code</a></p>
    </nav>
  </main>
</body>
</html>`;
}
