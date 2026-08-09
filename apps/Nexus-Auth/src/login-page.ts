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
  const errorBlock = options.error
    ? `<p class="err" role="alert">${escapeHtml(options.error)}</p>`
    : "";
  const destination = options.redirect
    ? `<p class="dest">You'll be returned to <code>${escapeHtml(options.redirect)}</code></p>`
    : "";

  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Sign in — Nexus Systems</title>
<style>
  :root { color-scheme: light dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh; display: grid; place-items: center;
    font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
    background: #0b0d12; color: #e8eaf0;
  }
  .card {
    width: min(92vw, 380px); padding: 32px;
    background: #141821; border: 1px solid #232838; border-radius: 14px;
  }
  h1 { margin: 0 0 4px; font-size: 1.3rem; }
  .sub { margin: 0 0 22px; color: #8b93a7; font-size: .88rem; }
  label { display: block; margin: 14px 0 6px; font-size: .82rem; color: #b6bdcd; }
  input[type=text], input[type=password] {
    width: 100%; padding: 10px 12px; border-radius: 8px;
    border: 1px solid #2b3145; background: #0e1119; color: inherit; font: inherit;
  }
  input:focus { outline: 2px solid #4c7dff; outline-offset: 1px; }
  button {
    width: 100%; margin-top: 20px; padding: 11px; border: 0; border-radius: 8px;
    background: #4c7dff; color: #fff; font: 600 15px/1 inherit; cursor: pointer;
  }
  button:hover { background: #3d6bec; }
  .err {
    margin: 14px 0 0; padding: 9px 11px; border-radius: 8px;
    background: #3a1720; border: 1px solid #6b2333; color: #ffb4c0; font-size: .85rem;
  }
  .dest { margin: 16px 0 0; font-size: .76rem; color: #6f778c; word-break: break-all; }
  code { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
</style>
</head>
<body>
  <main class="card">
    <h1>Sign in</h1>
    <p class="sub">One account for every Nexus app.</p>
    <form method="post" action="/login">
      ${redirectField}
      <label for="username">Username</label>
      <input id="username" name="username" type="text" autocomplete="username" autofocus required />
      <label for="password">Password</label>
      <input id="password" name="password" type="password" autocomplete="current-password" required />
      <button type="submit">Sign in</button>
      ${errorBlock}
    </form>
    ${destination}
  </main>
</body>
</html>`;
}
