/**
 * The three ways into the platform for someone who has no account yet.
 *
 *   /request  ask for access; shows the claim code once
 *   /claim    after approval, set a password with that code
 *   /join     redeem an operator's invite; skips the queue
 *
 * These used to be React pages on app.tnhc.dev, behind the sign-in gate, so a
 * newcomer was sent to a login form for an account they did not have yet; and
 * invites had no page at all. Like the major clouds, sign-up now lives on the
 * identity host, the one host the gate keeps public. Same construction as the
 * login page: server-rendered forms, no script, so there is no bundle to fail
 * and nothing for a script-injection to run in.
 */

export const ONBOARDING_LIMITS = {
  username: 64,
  email: 254,
  note: 500,
  password: 1024,
  code: 128,
} as const;

export function escapeHtml(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

/** Headers every onboarding response carries. The pages handle secrets (claim
 *  codes, recovery codes), so nothing may be cached, framed, or leak via Referer. */
export const ONBOARDING_HEADERS: Record<string, string> = {
  "content-type": "text/html; charset=utf-8",
  "cache-control": "no-store",
  pragma: "no-cache",
  "referrer-policy": "no-referrer",
  "x-content-type-options": "nosniff",
  "content-security-policy":
    "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'",
};

function renderShell(title: string, body: string): string {
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>${escapeHtml(title)} — Nexus Systems</title>
<style>
  :root {
    --bg: #0b0d12; --surface: #141821; --fg: #e8eaf0; --muted: #8b93a7;
    --subtle: #b6bdcd; --dim: #6f778c; --border: #2b3145; --border-focus: #232838;
    --input-bg: #0e1119; --accent: #4c7dff; --accent-hover: #3d6bec;
    --accent-subtle: #18203a; --accent-label: #c9d6ff;
    --err-bg: #3a1720; --err-border: #6b2333; --err-fg: #ffb4c0;
    --code-fg: #ffd98a; --link: #8fb0ff;
    color-scheme: dark;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100dvh; display: grid; place-items: center;
    font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
    background: var(--bg); color: var(--fg);
    padding: 24px 16px;
  }
  .card {
    width: min(100%, 420px); padding: clamp(18px, 5vw, 32px);
    background: var(--surface); border: 1px solid var(--border-focus); border-radius: 14px;
  }
  h1 { margin: 0 0 4px; font-size: 1.3rem; }
  .sub { margin: 0 0 18px; color: var(--muted); font-size: .88rem; }
  label { display: block; margin: 14px 0 6px; font-size: .82rem; color: var(--subtle); }
  input, textarea {
    width: 100%; padding: 10px 12px; border-radius: 8px;
    border: 1px solid var(--border); background: var(--input-bg); color: inherit; font: inherit;
    transition: border-color .15s;
  }
  textarea { min-height: 70px; resize: vertical; }
  input:focus, textarea:focus { outline: 2px solid var(--accent); outline-offset: 1px; border-color: var(--accent); }
  button, .button {
    display: block; width: 100%; margin-top: 20px; padding: 11px; border: 0; border-radius: 8px;
    background: var(--accent); color: #fff; font: 600 15px/1 inherit; cursor: pointer;
    text-align: center; text-decoration: none; transition: background .15s;
  }
  button:hover, .button:hover { background: var(--accent-hover); }
  button:focus-visible, .button:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
  .err {
    margin: 14px 0 0; padding: 9px 11px; border-radius: 8px;
    background: var(--err-bg); border: 1px solid var(--err-border); color: var(--err-fg); font-size: .85rem;
  }
  .secret {
    margin: 12px 0; padding: 12px; border-radius: 8px; background: var(--input-bg);
    border: 1px solid var(--border); font: 600 1rem ui-monospace, SFMono-Regular, Menlo, monospace;
    word-break: break-all; color: var(--code-fg);
  }
  /* One code per row: a 32-character code never fits two columns on a phone,
     and a code broken across lines is one nobody can copy correctly. The font
     shrinks a little on narrow screens instead. Tapping the block selects all. */
  .codes {
    margin: 12px 0; padding: 12px 12px 12px 2.6em; border-radius: 8px;
    background: var(--input-bg); border: 1px solid var(--border); color: var(--code-fg);
    font: clamp(.7rem, 3.4vw, .92rem)/1.7 ui-monospace, SFMono-Regular, Menlo, monospace;
    overflow-x: auto; user-select: all; -webkit-user-select: all;
  }
  .codes li { white-space: nowrap; }
  .codes li::marker { color: var(--dim); }
  .button.secondary { background: transparent; border: 1px solid var(--accent); color: var(--accent-label); }
  .button.secondary:hover { background: var(--accent-subtle); }
  .note { font-size: .82rem; color: var(--muted); }
  .links { margin: 18px 0 0; font-size: .8rem; color: var(--muted); }
  a { color: var(--link); text-underline-offset: 2px; }
  .hint { font-size: .75rem; color: var(--dim); margin: 4px 0 0; }
</style>
</head>
<body>
  <main class="card">
${body}
  </main>
</body>
</html>`;
}

function renderError(error: string | undefined): string {
  return error ? `<p class="err" role="alert">${escapeHtml(error)}</p>` : "";
}

const passwordFields = (minLength: number) => `
      <label for="password">Password</label>
      <input id="password" name="password" type="password" autocomplete="new-password" minlength="${minLength}" maxlength="${ONBOARDING_LIMITS.password}" aria-describedby="pw-hint" required />
      <p class="hint" id="pw-hint">At least ${minLength} characters.</p>
      <label for="confirm">Confirm password</label>
      <input id="confirm" name="confirm" type="password" autocomplete="new-password" minlength="${minLength}" maxlength="${ONBOARDING_LIMITS.password}" aria-describedby="pw-hint" required />`;

const otherWaysIn = `
    <p class="links">Already have an account? <a href="/login">Sign in</a></p>`;

export function renderRequestPage(options: { username?: string; email?: string; note?: string; error?: string }): string {
  return renderShell("Request access", `
    <h1>Request access</h1>
    <p class="sub">Nexus is invite-only while it grows. Ask for an account and an operator will review it.</p>
    <form method="post" action="/request">
      <label for="username">Username</label>
      <input id="username" name="username" type="text" autocomplete="username" maxlength="${ONBOARDING_LIMITS.username}" value="${escapeHtml(options.username ?? "")}" required autofocus />
      <label for="email">Email</label>
      <input id="email" name="email" type="email" autocomplete="email" maxlength="${ONBOARDING_LIMITS.email}" value="${escapeHtml(options.email ?? "")}" required />
      <label for="note">Anything you'd like us to know (optional)</label>
      <textarea id="note" name="note" maxlength="${ONBOARDING_LIMITS.note}">${escapeHtml(options.note ?? "")}</textarea>
      <button type="submit">Request access</button>
      ${renderError(options.error)}
    </form>
    <p class="links">Have an invite? <a href="/join">Join with it</a> · Already approved? <a href="/claim">Claim your account</a></p>${otherWaysIn}`);
}

export function renderRequestedPage(options: { email: string; claimCode: string }): string {
  const claimLink = `/claim?email=${encodeURIComponent(options.email)}`;
  return renderShell("Request received", `
    <h1>Request received</h1>
    <p class="sub">Keep this claim code. It is shown once and cannot be recovered.</p>
    <div class="secret" data-claim-code="${escapeHtml(options.claimCode)}">${escapeHtml(options.claimCode)}</div>
    <p class="note">Once an operator approves your request, come back to
      <a href="${escapeHtml(claimLink)}">claim your account</a> with this code and choose a password.</p>`);
}

export function renderClaimPage(options: { email?: string; minPasswordLength: number; error?: string }): string {
  return renderShell("Claim your account", `
    <h1>Claim your account</h1>
    <p class="sub">Your request was approved. Use the claim code you were given to set a password.</p>
    <form method="post" action="/claim">
      <label for="email">Email</label>
      <input id="email" name="email" type="email" autocomplete="email" maxlength="${ONBOARDING_LIMITS.email}" value="${escapeHtml(options.email ?? "")}" required />
      <label for="claimCode">Claim code</label>
      <input id="claimCode" name="claimCode" type="text" autocomplete="off" spellcheck="false" maxlength="${ONBOARDING_LIMITS.code}" required />
      ${passwordFields(options.minPasswordLength)}
      <button type="submit">Claim account</button>
      ${renderError(options.error)}
    </form>
    <p class="links">No claim code? <a href="/request">Request access</a> · Have an invite? <a href="/join">Join with it</a></p>${otherWaysIn}`);
}

export function renderJoinPage(options: { code?: string; username?: string; email?: string; minPasswordLength: number; error?: string }): string {
  return renderShell("Join with an invite", `
    <h1>Join Nexus</h1>
    <p class="sub">You were invited. Choose a username and password to create your account.</p>
    <form method="post" action="/join">
      <label for="code">Invite code</label>
      <input id="code" name="code" type="text" autocomplete="off" spellcheck="false" maxlength="${ONBOARDING_LIMITS.code}" value="${escapeHtml(options.code ?? "")}" required />
      <label for="username">Username</label>
      <input id="username" name="username" type="text" autocomplete="username" maxlength="${ONBOARDING_LIMITS.username}" value="${escapeHtml(options.username ?? "")}" required />
      <label for="email">Email</label>
      <input id="email" name="email" type="email" autocomplete="email" maxlength="${ONBOARDING_LIMITS.email}" value="${escapeHtml(options.email ?? "")}" required />
      ${passwordFields(options.minPasswordLength)}
      <button type="submit">Create account</button>
      ${renderError(options.error)}
    </form>
    <p class="links">No invite? <a href="/request">Request access</a></p>${otherWaysIn}`);
}

export function renderRecoverPage(options: { email?: string; error?: string }): string {
  return renderShell("Recover your account", `
    <h1>Lost your password?</h1>
    <p class="sub">Use one of the recovery codes you saved when you created your account. Each code works once.</p>
    <form method="post" action="/recover">
      <label for="email">Email</label>
      <input id="email" name="email" type="email" autocomplete="email" maxlength="${ONBOARDING_LIMITS.email}" value="${escapeHtml(options.email ?? "")}" required autofocus />
      <label for="code">Recovery code</label>
      <input id="code" name="code" type="text" autocomplete="off" spellcheck="false" maxlength="${ONBOARDING_LIMITS.code}" required />
      <button type="submit">Continue</button>
      ${renderError(options.error)}
    </form>
    <p class="links">No codes left? Ask whoever runs this Nexus node to help you get back in.</p>${otherWaysIn}`);
}

export function renderResetPasswordPage(options: { grant: string; minPasswordLength: number; error?: string }): string {
  return renderShell("Choose a new password", `
    <h1>Choose a new password</h1>
    <p class="sub">Your recovery code was accepted and has been used up. Set a new password to finish. This step expires in 15 minutes.</p>
    <form method="post" action="/recover/password">
      <input type="hidden" name="grant" value="${escapeHtml(options.grant)}" />
      ${passwordFields(options.minPasswordLength)}
      <button type="submit">Set password and sign in</button>
      ${renderError(options.error)}
    </form>`);
}

export function renderRecoveredPage(options: { remaining: number; continueTo: string }): string {
  const left = `${options.remaining} recovery code${options.remaining === 1 ? "" : "s"} left`;
  return renderShell("Password changed", `
    <h1>You're back in</h1>
    <p class="sub">Your password was changed and every other session was signed out.</p>
    <p class="note">You have ${left}.${options.remaining <= 3 ? " Create a fresh set from your Account page soon." : ""}</p>
    <a class="button" href="${escapeHtml(options.continueTo)}">Continue to Nexus</a>`);
}

/** Shown once, straight after an account is created. The person is already
 *  signed in; recovery codes are the only way back in without the password. */
/** The codes as a plain-text file, built into a data: link so the download
 *  needs no script (these pages run none) and no second request: the codes are
 *  stored only as hashes, so nothing could serve them again later. */
function buildRecoveryCodesDownload(username: string, recoveryCodes: string[]): string {
  const text = [
    `Nexus recovery codes for ${username}`,
    `Created ${new Date().toISOString().slice(0, 10)} (UTC)`,
    "",
    "Each code signs you in once if you lose your password.",
    "Keep this file somewhere safe and private, and delete it once the codes are stored elsewhere.",
    "",
    ...recoveryCodes.map((code, i) => `${String(i + 1).padStart(2, " ")}. ${code}`),
    "",
  ].join("\n");
  return `data:text/plain;charset=utf-8,${encodeURIComponent(text)}`;
}

export function renderWelcomePage(options: { username: string; recoveryCodes: string[]; continueTo: string }): string {
  const codes = options.recoveryCodes
    .map((code) => `<li data-recovery-code>${escapeHtml(code)}</li>`)
    .join("");
  const fileName = `nexus-recovery-codes-${options.username.toLowerCase().replace(/[^a-z0-9]/g, "")}.txt`;
  return renderShell("Welcome", `
    <h1>Welcome, ${escapeHtml(options.username)}</h1>
    <p class="sub">Your account is ready and you are signed in.</p>
    <p class="note">Save these recovery codes somewhere safe. Each one signs you in once if you lose your
      password. They are shown only now.</p>
    <ol class="codes" aria-label="Recovery codes">${codes}</ol>
    <a class="button secondary" download="${escapeHtml(fileName)}" href="${escapeHtml(buildRecoveryCodesDownload(options.username, options.recoveryCodes))}">Download as a text file</a>
    <a class="button" href="${escapeHtml(options.continueTo)}">Continue to Nexus</a>`);
}
