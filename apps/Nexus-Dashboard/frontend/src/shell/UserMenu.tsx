import { useState, useRef, useEffect } from "react";
import { Link } from "react-router-dom";
import { Avatar } from "../../../../../packages/nexus-design/src/components/ui/avatar";
import { Badge } from "../../../../../packages/nexus-design/src/components/ui/badge";
import { logout } from "../api";
import { useTheme, THEMES, type Theme } from "./useTheme";
import { useDensity } from "./useDensity";

export interface UserMenuProps {
  user: {
    id?: string;
    username: string;
    email: string;
    role?: string;
    did?: string;
  };
}

export default function UserMenu({ user }: UserMenuProps) {
  const [open, setOpen] = useState(false);
  const [loggingOut, setLoggingOut] = useState(false);
  const [copiedId, setCopiedId] = useState(false);
  const [copiedDid, setCopiedDid] = useState(false);
  const [showSessionDetails, setShowSessionDetails] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);
  const [theme, setTheme] = useTheme();
  const [density, setDensity] = useDensity();

  const isOperator = user.role === "founder" || user.role === "admin";
  const displayName = user.username || user.email;
  const sovereignDid = user.did || `did:phantom:${(user.id || user.username || "anon").slice(0, 16)}`;

  async function handleCopyId() {
    if (typeof navigator !== "undefined" && navigator.clipboard) {
      await navigator.clipboard.writeText(user.id || user.email || user.username);
      setCopiedId(true);
      setTimeout(() => setCopiedId(false), 2000);
    }
  }

  async function handleCopyDid() {
    if (typeof navigator !== "undefined" && navigator.clipboard) {
      await navigator.clipboard.writeText(sovereignDid);
      setCopiedDid(true);
      setTimeout(() => setCopiedDid(false), 2000);
    }
  }

  function handleHardReload() {
    if (typeof window !== "undefined") {
      const win = window as unknown as Window & { caches?: CacheStorage };
      if (win.caches) {
        win.caches
          .keys()
          .then((keys) => Promise.all(keys.map((k) => win.caches!.delete(k))))
          .finally(() => {
            win.location.reload();
          });
      } else {
        win.location.reload();
      }
    }
  }

  useEffect(() => {
    function handleClickOutside(e: MouseEvent) {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        setOpen(false);
      }
    }
    function handleKeyDown(e: KeyboardEvent) {
      if (e.key === "Escape") setOpen(false);
    }
    if (open) {
      document.addEventListener("mousedown", handleClickOutside);
      document.addEventListener("keydown", handleKeyDown);
    }
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
      document.removeEventListener("keydown", handleKeyDown);
    };
  }, [open]);

  async function handleLogout() {
    setLoggingOut(true);
    await logout();
  }

  return (
    <div className="relative" ref={menuRef}>
      <button
        type="button"
        onClick={() => setOpen((prev) => !prev)}
        aria-haspopup="true"
        aria-expanded={open}
        aria-label={`User menu for ${displayName}`}
        title={user.email}
        className="flex items-center gap-2 rounded-md px-2 py-1 text-sm text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
      >
        <Avatar label={displayName} />
        <span className="hidden max-w-[10rem] truncate text-left sm:inline font-medium text-zinc-200">
          {displayName}
        </span>
        <svg
          className={`h-4 w-4 shrink-0 transition-transform ${open ? "rotate-180" : ""}`}
          fill="none"
          viewBox="0 0 24 24"
          stroke="currentColor"
          strokeWidth={2}
          aria-hidden="true"
        >
          <path strokeLinecap="round" strokeLinejoin="round" d="M19 9l-7 7-7-7" />
        </svg>
      </button>

      {open && (
        <div
          role="menu"
          aria-label="User navigation"
          className="absolute right-0 top-full mt-2 w-80 origin-top-right rounded-xl border border-zinc-800 bg-zinc-950 p-2 shadow-2xl ring-1 ring-black/50 z-50 text-zinc-200"
        >
          {/* Header profile info */}
          <div className="p-3 border-b border-zinc-800/80">
            <div className="flex items-start gap-3">
              <Avatar label={displayName} className="h-10 w-10 text-base shrink-0" />
              <div className="min-w-0 flex-1">
                <div className="flex items-center justify-between gap-1">
                  <span className="truncate font-semibold text-zinc-100">{displayName}</span>
                  <Badge variant={isOperator ? "accent" : "default"} className="text-[10px] uppercase tracking-wider">
                    {user.role ?? "member"}
                  </Badge>
                </div>
                <p className="truncate text-xs text-zinc-400 mt-0.5">{user.email}</p>
                <div className="mt-1.5 flex items-center gap-1.5 text-[11px] text-emerald-400">
                  <span className="h-1.5 w-1.5 rounded-full bg-emerald-400 animate-pulse" />
                  <span>Session Active · Sovereign Auth</span>
                </div>
              </div>
            </div>

            {/* Sovereign Identity DID Badge */}
            <div className="mt-2.5 flex items-center justify-between rounded-md bg-zinc-900/80 px-2 py-1 text-[11px]">
              <span className="truncate font-mono text-zinc-400" title={sovereignDid}>
                {sovereignDid}
              </span>
              <button
                type="button"
                onClick={() => void handleCopyDid()}
                className="ml-2 text-zinc-400 hover:text-zinc-200 text-[10px] font-medium"
                title="Copy Sovereign DID"
              >
                {copiedDid ? "✓ Copied" : "Copy"}
              </button>
            </div>

            {/* Collapsible Session Details */}
            <div className="mt-2 pt-2 border-t border-zinc-900">
              <button
                type="button"
                onClick={() => setShowSessionDetails((v) => !v)}
                className="w-full flex items-center justify-between text-[11px] text-zinc-400 hover:text-zinc-200"
              >
                <span>Active Session Details</span>
                <span>{showSessionDetails ? "▲" : "▼"}</span>
              </button>
              {showSessionDetails && (
                <div className="mt-2 space-y-1.5 rounded bg-zinc-900/90 p-2 text-[10px] text-zinc-300 font-mono">
                  <div className="flex justify-between">
                    <span className="text-zinc-500">Host:</span>
                    <span>{typeof window !== "undefined" ? window.location.host : "app.tnhc.dev"}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-zinc-500">Auth Method:</span>
                    <span>Ed25519 DID Cookie</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-zinc-500">Cookie Flags:</span>
                    <span className="text-emerald-400">HttpOnly · SameSite=Lax</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-zinc-500">Protocol:</span>
                    <span>{typeof window !== "undefined" ? window.location.protocol : "https:"}</span>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Primary Navigation */}
          <div className="py-1">
            <Link
              to="/account"
              onClick={() => setOpen(false)}
              role="menuitem"
              className="flex items-center justify-between rounded-md px-3 py-2 text-sm text-zinc-300 hover:bg-zinc-900 hover:text-white"
            >
              <div className="flex items-center gap-2.5">
                <svg className="h-4 w-4 text-zinc-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M16 7a4 4 0 11-8 0 4 4 0 018 0zM12 14a7 7 0 00-7 7h14a7 7 0 00-7-7z" />
                </svg>
                <span>Account & Security</span>
              </div>
              <span className="text-xs text-zinc-500">Manage</span>
            </Link>

            <Link
              to="/docs"
              onClick={() => setOpen(false)}
              role="menuitem"
              className="flex items-center justify-between rounded-md px-3 py-2 text-sm text-zinc-300 hover:bg-zinc-900 hover:text-white"
            >
              <div className="flex items-center gap-2.5">
                <svg className="h-4 w-4 text-zinc-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 6.253v13m0-13C10.832 5.477 9.246 5 7.5 5S4.168 5.477 3 6.253v13C4.168 18.477 5.754 18 7.5 18s3.332.477 4.5 1.253m0-13C13.168 5.477 14.754 5 16.5 5c1.747 0 3.332.477 4.5 1.253v13C19.832 18.477 18.247 18 16.5 18c-1.746 0-3.332.477-4.5 1.253" />
                </svg>
                <span>Documentation & Guides</span>
              </div>
              <span className="text-xs text-zinc-500">Hub</span>
            </Link>

            <Link
              to="/report"
              onClick={() => setOpen(false)}
              role="menuitem"
              className="flex items-center justify-between rounded-md px-3 py-2 text-sm text-zinc-300 hover:bg-zinc-900 hover:text-white"
            >
              <div className="flex items-center gap-2.5">
                <svg className="h-4 w-4 text-zinc-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
                </svg>
                <span>Report an Issue</span>
              </div>
              <span className="text-xs text-zinc-500">Feedback</span>
            </Link>

            {isOperator && (
              <Link
                to="/admin"
                onClick={() => setOpen(false)}
                role="menuitem"
                className="flex items-center justify-between rounded-md px-3 py-2 text-sm text-zinc-300 hover:bg-zinc-900 hover:text-white"
              >
                <div className="flex items-center gap-2.5">
                  <svg className="h-4 w-4 text-accent" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                  </svg>
                  <span>Operator Console</span>
                </div>
                <span className="text-xs text-accent">Admin</span>
              </Link>
            )}
          </div>

          {/* Quick Actions / Developer & User QoL */}
          <div className="py-1 border-t border-zinc-800/80">
            <button
              type="button"
              onClick={() => void handleCopyId()}
              className="w-full flex items-center justify-between rounded-md px-3 py-1.5 text-xs text-zinc-400 hover:bg-zinc-900 hover:text-zinc-200"
            >
              <span>{copiedId ? "✓ Copied to clipboard" : "Copy Account UUID"}</span>
              <span className="text-[10px] text-zinc-500 font-mono">{(user.id || user.email).slice(0, 10)}…</span>
            </button>
            <button
              type="button"
              onClick={handleHardReload}
              className="w-full flex items-center justify-between rounded-md px-3 py-1.5 text-xs text-zinc-400 hover:bg-zinc-900 hover:text-zinc-200"
              title="Clears browser caches and fetches the latest bundles"
            >
              <span>Purge Cache & Reload</span>
              <span className="text-[10px] text-zinc-500">Ctrl+F5</span>
            </button>
          </div>

          <div className="my-1 border-t border-zinc-800/80" />

          {/* Appearance: Theme Picker */}
          <div className="px-3 py-2">
            <span className="block text-[11px] font-medium uppercase tracking-wider text-zinc-500 mb-2">
              Theme Palette
            </span>
            <div className="grid grid-cols-2 gap-1.5">
              {THEMES.map((t) => (
                <button
                  key={t.id}
                  type="button"
                  onClick={() => setTheme(t.id)}
                  aria-pressed={theme === t.id}
                  className={`flex items-center gap-2 rounded-md border px-2 py-1.5 text-xs text-left transition-colors ${
                    theme === t.id
                      ? "border-accent bg-accent/10 text-accent font-medium"
                      : "border-zinc-800 bg-zinc-900/60 text-zinc-400 hover:border-zinc-700 hover:text-zinc-200"
                  }`}
                >
                  <span
                    className="h-2.5 w-2.5 rounded-full border border-white/20 shrink-0"
                    style={{ backgroundColor: t.hex }}
                    aria-hidden="true"
                  />
                  <span className="truncate">{t.name}</span>
                </button>
              ))}
            </div>
          </div>

          {/* Density Toggle */}
          <div className="px-3 py-2 border-t border-zinc-800/80">
            <div className="flex items-center justify-between">
              <span className="text-xs text-zinc-400 font-medium">Density</span>
              <div className="inline-flex rounded-md border border-zinc-800 bg-zinc-900 p-0.5">
                <button
                  type="button"
                  onClick={() => setDensity("balanced")}
                  aria-pressed={density === "balanced"}
                  className={`rounded px-2 py-0.5 text-xs transition-colors ${
                    density === "balanced"
                      ? "bg-zinc-800 text-zinc-100 font-medium"
                      : "text-zinc-400 hover:text-zinc-200"
                  }`}
                >
                  Balanced
                </button>
                <button
                  type="button"
                  onClick={() => setDensity("compact")}
                  aria-pressed={density === "compact"}
                  className={`rounded px-2 py-0.5 text-xs transition-colors ${
                    density === "compact"
                      ? "bg-zinc-800 text-zinc-100 font-medium"
                      : "text-zinc-400 hover:text-zinc-200"
                  }`}
                >
                  Compact
                </button>
              </div>
            </div>
          </div>

          <div className="my-1 border-t border-zinc-800/80" />

          {/* Sign Out Button */}
          <div className="p-1">
            <button
              type="button"
              role="menuitem"
              disabled={loggingOut}
              onClick={() => void handleLogout()}
              className="w-full flex items-center justify-between rounded-md px-3 py-2.5 text-sm font-medium text-red-400 hover:bg-red-950/40 hover:text-red-300 transition-colors disabled:opacity-50"
            >
              <div className="flex items-center gap-2">
                <svg className="h-4 w-4 shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" />
                </svg>
                <span>{loggingOut ? "Signing out…" : "Log Out of Platform"}</span>
              </div>
              <span className="text-[10px] text-red-500 font-mono">SSO</span>
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
