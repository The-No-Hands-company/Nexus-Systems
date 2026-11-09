import { escapeHtml, ONBOARDING_HEADERS } from "./onboarding-page";
import type { SafeUser, Session } from "./types";

export interface AccountPageOptions {
  user: SafeUser;
  sessions: Session[];
  currentSessionId?: string;
  remainingRecoveryCodes?: number;
  newRecoveryCodes?: string[];
  totpSecretCandidate?: string;
  totpUri?: string;
  notice?: { kind: "success" | "error"; message: string };
  dashboardUrl: string;
}

export function renderAccountPage(options: AccountPageOptions): string {
  const {
    user,
    sessions,
    currentSessionId,
    remainingRecoveryCodes,
    newRecoveryCodes,
    totpSecretCandidate,
    totpUri,
    notice,
    dashboardUrl,
  } = options;

  const noticeHtml = notice
    ? `<div class="notice notice-${notice.kind}">${escapeHtml(notice.message)}</div>`
    : "";

  const recoveryHtml = newRecoveryCodes && newRecoveryCodes.length > 0
    ? `
      <div class="card card-highlight">
        <h3>New Recovery Codes</h3>
        <p class="desc">Save these one-time codes immediately. They will not be displayed again.</p>
        <ol class="code-grid">
          ${newRecoveryCodes.map((c) => `<li><code>${escapeHtml(c)}</code></li>`).join("")}
        </ol>
      </div>
    `
    : `
      <div class="card">
        <h3>Recovery Codes</h3>
        <p class="desc">You have <strong>${remainingRecoveryCodes ?? 0}</strong> active one-time recovery codes remaining.</p>
        <form method="post" action="/account/recovery-codes/regenerate">
          <button type="submit" class="btn btn-secondary" onclick="return confirm('Regenerating codes invalidates all existing codes. Continue?')">Regenerate Recovery Codes</button>
        </form>
      </div>
    `;

  const totpHtml = user.totpEnabled
    ? `
      <div class="card">
        <div class="row-between">
          <div>
            <h3>Two-Factor Authentication (TOTP)</h3>
            <p class="desc">Status: <span class="badge badge-success">Active & Enforced</span></p>
          </div>
          <form method="post" action="/account/2fa/disable">
            <button type="submit" class="btn btn-danger" onclick="return confirm('Are you sure you want to disable 2FA?')">Disable 2FA</button>
          </form>
        </div>
      </div>
    `
    : totpSecretCandidate
    ? `
      <div class="card card-highlight">
        <h3>Setup Authenticator App</h3>
        <p class="desc">Scan the key below into your authenticator app (1Password, Google Authenticator, Bitwarden, etc.):</p>
        <div class="code-box">
          <code>${escapeHtml(totpSecretCandidate)}</code>
        </div>
        ${totpUri ? `<p class="desc" style="word-break: break-all; font-size: 0.8rem; color: var(--muted);">${escapeHtml(totpUri)}</p>` : ""}
        <form method="post" action="/account/2fa/verify" class="form-compact">
          <input type="hidden" name="secret" value="${escapeHtml(totpSecretCandidate)}" />
          <div class="field">
            <label for="totp-code">Enter 6-digit verification code</label>
            <input type="text" id="totp-code" name="code" placeholder="123456" pattern="\\d{6}" maxlength="6" required autocomplete="one-time-code" />
          </div>
          <button type="submit" class="btn btn-primary">Verify & Enable 2FA</button>
        </form>
      </div>
    `
    : `
      <div class="card">
        <div class="row-between">
          <div>
            <h3>Two-Factor Authentication (TOTP)</h3>
            <p class="desc">Protect your account with standard 6-digit TOTP authenticator codes.</p>
          </div>
          <form method="post" action="/account/2fa/setup">
            <button type="submit" class="btn btn-primary">Enable 2FA</button>
          </form>
        </div>
      </div>
    `;

  const sessionsRows = sessions.map((s) => {
    const isCurrent = s.id === currentSessionId;
    return `
      <tr>
        <td><code>${escapeHtml(s.deviceId)}</code> ${isCurrent ? '<span class="badge badge-current">Current Device</span>' : ""}</td>
        <td>${escapeHtml(new Date(s.createdAt).toLocaleDateString())} ${escapeHtml(new Date(s.createdAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }))}</td>
        <td>
          <form method="post" action="/account/sessions/${encodeURIComponent(s.id)}/revoke">
            <button type="submit" class="btn btn-xs ${isCurrent ? 'btn-danger' : 'btn-secondary'}">
              ${isCurrent ? 'Sign Out' : 'Revoke'}
            </button>
          </form>
        </td>
      </tr>
    `;
  }).join("");

  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
<title>Account Settings — Nexus Systems</title>
<style>
  :root {
    --bg: #0b0d12; --surface: #141821; --surface-card: #191e2b; --fg: #e8eaf0; --muted: #8b93a7;
    --subtle: #b6bdcd; --border: #2b3145; --input-bg: #0e1119; --accent: #4c7dff;
    --accent-hover: #3d6bec; --success: #22c55e; --danger: #ef4444; --danger-hover: #dc2626;
    color-scheme: dark;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg); color: var(--fg);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    line-height: 1.5; padding: 2rem 1rem;
  }
  .container { max-width: 800px; margin: 0 auto; }
  .header {
    display: flex; justify-content: space-between; align-items: center;
    border-bottom: 1px solid var(--border); padding-bottom: 1.5rem; margin-bottom: 2rem;
  }
  .header-titles h1 { font-size: 1.5rem; font-weight: 600; }
  .header-titles p { color: var(--muted); font-size: 0.9rem; }
  .header-actions { display: flex; gap: 0.75rem; align-items: center; }
  .card {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 8px; padding: 1.5rem; margin-bottom: 1.5rem;
  }
  .card-highlight { border-color: var(--accent); background: #131a30; }
  .card h3 { font-size: 1.15rem; margin-bottom: 0.5rem; }
  .desc { color: var(--muted); font-size: 0.9rem; margin-bottom: 1rem; }
  .row-between { display: flex; justify-content: space-between; align-items: center; }
  .badge {
    display: inline-block; padding: 0.2rem 0.5rem; border-radius: 4px;
    font-size: 0.75rem; font-weight: 600; text-transform: uppercase;
  }
  .badge-success { background: rgba(34, 197, 94, 0.2); color: #4ade80; border: 1px solid #22c55e; }
  .badge-current { background: rgba(76, 125, 255, 0.2); color: #93b4ff; border: 1px solid #4c7dff; }
  .notice {
    padding: 0.75rem 1rem; border-radius: 6px; margin-bottom: 1.5rem; font-size: 0.9rem;
  }
  .notice-success { background: rgba(34, 197, 94, 0.15); border: 1px solid #22c55e; color: #86efac; }
  .notice-error { background: rgba(239, 68, 68, 0.15); border: 1px solid #ef4444; color: #fca5a5; }
  .field { margin-bottom: 1rem; }
  .field label { display: block; font-size: 0.85rem; color: var(--subtle); margin-bottom: 0.4rem; }
  input[type="text"], input[type="email"], input[type="password"] {
    width: 100%; padding: 0.6rem 0.8rem; background: var(--input-bg);
    border: 1px solid var(--border); border-radius: 6px; color: var(--fg);
    font-size: 0.95rem;
  }
  input:focus { outline: none; border-color: var(--accent); }
  .btn {
    display: inline-flex; align-items: center; justify-content: center;
    padding: 0.55rem 1.1rem; border-radius: 6px; font-size: 0.85rem; font-weight: 500;
    cursor: pointer; border: none; text-decoration: none; transition: background 0.15s;
  }
  .btn-primary { background: var(--accent); color: #fff; }
  .btn-primary:hover { background: var(--accent-hover); }
  .btn-secondary { background: var(--surface-card); color: var(--fg); border: 1px solid var(--border); }
  .btn-secondary:hover { background: #252c40; }
  .btn-danger { background: rgba(239, 68, 68, 0.2); color: #fca5a5; border: 1px solid #ef4444; }
  .btn-danger:hover { background: var(--danger); color: #fff; }
  .btn-xs { padding: 0.3rem 0.6rem; font-size: 0.75rem; }
  .code-grid {
    display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
    gap: 0.5rem; margin-top: 1rem; list-style: none;
  }
  .code-grid code {
    display: block; padding: 0.4rem 0.6rem; background: var(--input-bg);
    border: 1px solid var(--border); border-radius: 4px; font-family: monospace;
    font-size: 0.85rem; text-align: center; color: #fde047;
  }
  .code-box {
    padding: 0.8rem; background: var(--input-bg); border: 1px solid var(--border);
    border-radius: 6px; font-family: monospace; font-size: 1.1rem;
    color: #93b4ff; letter-spacing: 2px; margin-bottom: 1rem; text-align: center;
  }
  .table-responsive { width: 100%; overflow-x: auto; }
  table { width: 100%; border-collapse: collapse; font-size: 0.9rem; margin-top: 0.5rem; }
  th, td { padding: 0.75rem 0.5rem; text-align: left; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-size: 0.8rem; text-transform: uppercase; }
  .sessions-actions { display: flex; justify-content: flex-end; margin-top: 1rem; }
</style>
</head>
<body>
  <div class="container">
    <header class="header">
      <div class="header-titles">
        <h1>Account &amp; Security</h1>
        <p>Signed in as <strong>${escapeHtml(user.username)}</strong> (${escapeHtml(user.email)}) · <span class="badge badge-current">${escapeHtml(user.role)}</span></p>
      </div>
      <div class="header-actions">
        <a href="${escapeHtml(dashboardUrl)}" class="btn btn-secondary">Dashboard</a>
        <a href="/logout" class="btn btn-secondary">Sign Out</a>
      </div>
    </header>

    ${noticeHtml}

    <!-- Profile Management -->
    <div class="card">
      <h3>Profile Details</h3>
      <form method="post" action="/account/profile">
        <div class="field">
          <label for="displayName">Display Name</label>
          <input type="text" id="displayName" name="displayName" value="${escapeHtml(user.displayName ?? user.username)}" maxlength="64" />
        </div>
        <div class="field">
          <label for="email">Email Address</label>
          <input type="email" id="email" name="email" value="${escapeHtml(user.email)}" required />
        </div>
        <button type="submit" class="btn btn-primary">Save Profile</button>
      </form>
    </div>

    <!-- Password Change -->
    <div class="card">
      <h3>Change Password</h3>
      <form method="post" action="/account/password">
        <div class="field">
          <label for="currentPassword">Current Password</label>
          <input type="password" id="currentPassword" name="currentPassword" required autocomplete="current-password" />
        </div>
        <div class="field">
          <label for="newPassword">New Password (at least 12 characters)</label>
          <input type="password" id="newPassword" name="newPassword" minlength="12" required autocomplete="new-password" />
        </div>
        <button type="submit" class="btn btn-primary">Update Password</button>
      </form>
    </div>

    <!-- Two-Factor Authentication -->
    ${totpHtml}

    <!-- Recovery Codes -->
    ${recoveryHtml}

    <!-- Active Devices & Sessions -->
    <div class="card">
      <div class="row-between">
        <h3>Active Devices &amp; Sessions</h3>
        <span class="desc" style="margin: 0;">${sessions.length} active</span>
      </div>
      <p class="desc">Every active sign-in to Nexus Systems across your devices.</p>
      <div class="table-responsive">
        <table>
          <thead>
            <tr>
              <th>Device</th>
              <th>Signed In</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            ${sessionsRows}
          </tbody>
        </table>
      </div>
      <div class="sessions-actions">
        <form method="post" action="/account/sessions/revoke-others">
          <button type="submit" class="btn btn-secondary" ${sessions.length <= 1 ? "disabled" : ""}>
            Sign Out All Other Devices
          </button>
        </form>
      </div>
    </div>
  </div>
</body>
</html>`;
}
