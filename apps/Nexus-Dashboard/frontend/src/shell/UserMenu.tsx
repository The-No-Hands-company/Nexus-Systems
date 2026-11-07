import { useState, useRef, useEffect } from "react";
import { Link } from "react-router-dom";
import { Avatar } from "../../../../../packages/nexus-design/src/components/ui/avatar";
import { Badge } from "../../../../../packages/nexus-design/src/components/ui/badge";
import { logout } from "../api";
import { useTheme, THEMES, type Theme } from "./useTheme";
import { useDensity } from "./useDensity";

export interface UserMenuProps {
  user: {
    username: string;
    email: string;
    role?: string;
  };
}

export default function UserMenu({ user }: UserMenuProps) {
  const [open, setOpen] = useState(false);
  const [loggingOut, setLoggingOut] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);
  const [theme, setTheme] = useTheme();
  const [density, setDensity] = useDensity();

  const isOperator = user.role === "founder" || user.role === "admin";
  const displayName = user.username || user.email;

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
          className="absolute right-0 top-full mt-2 w-72 origin-top-right rounded-lg border border-zinc-800 bg-zinc-950 p-2 shadow-2xl ring-1 ring-black/50 z-50 text-zinc-200"
        >
          {/* Header profile info */}
          <div className="flex items-start gap-3 p-3 border-b border-zinc-800/80 pb-3">
            <Avatar label={displayName} className="h-10 w-10 text-base" />
            <div className="min-w-0 flex-1">
              <div className="flex items-center justify-between gap-1">
                <span className="truncate font-semibold text-zinc-100">{displayName}</span>
                <Badge variant={isOperator ? "accent" : "default"} className="text-[10px] uppercase tracking-wider">
                  {user.role ?? "member"}
                </Badge>
              </div>
              <p className="truncate text-xs text-zinc-400 mt-0.5">{user.email}</p>
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
              <span>Account & Security</span>
              <span className="text-xs text-zinc-500">Manage</span>
            </Link>

            {isOperator && (
              <Link
                to="/admin"
                onClick={() => setOpen(false)}
                role="menuitem"
                className="flex items-center justify-between rounded-md px-3 py-2 text-sm text-zinc-300 hover:bg-zinc-900 hover:text-white"
              >
                <span>Operator Console</span>
                <span className="text-xs text-accent">Admin</span>
              </Link>
            )}
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
              className="w-full flex items-center justify-between rounded-md px-3 py-2 text-sm text-red-400 hover:bg-red-950/40 hover:text-red-300 transition-colors disabled:opacity-50"
            >
              <span>{loggingOut ? "Signing out…" : "Sign out of Nexus"}</span>
              <svg className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" />
              </svg>
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
