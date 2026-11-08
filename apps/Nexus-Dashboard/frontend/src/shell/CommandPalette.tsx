import { useState, useEffect, useRef, useMemo } from "react";
import { useNavigate } from "react-router-dom";
import type { AppEntry } from "../api";
import { useTheme, THEMES } from "./useTheme";
import { useDensity } from "./useDensity";
import { logout } from "../api";

export interface CommandPaletteProps {
  open: boolean;
  onClose: () => void;
  apps: AppEntry[];
  user?: {
    id?: string;
    username?: string;
    email?: string;
    role?: string;
  } | null;
}

type SearchItem = {
  id: string;
  type: "app" | "doc" | "action";
  title: string;
  subtitle: string;
  category?: string;
  action: () => void;
  badge?: string;
};

const DOC_TOPICS = [
  { id: "overview", title: "Platform Overview & Sovereignty", category: "Architecture", path: "/docs#overview" },
  { id: "apps-inventory", title: "The 26 Core Platform Applications", category: "Catalog", path: "/docs#apps-inventory" },
  { id: "nexus-deploy", title: "Nexus Deploy & Rootless Sandboxing", category: "Developer", path: "/docs#nexus-deploy" },
  { id: "auth-did", title: "Sovereign Auth & Ed25519 DIDs", category: "Security", path: "/docs#auth-did" },
  { id: "document-suite", title: "Knowledge & Document Suite (Docs, Notes, Wiki)", category: "Work", path: "/docs#document-suite" },
  { id: "creative-suite", title: "Creative Suite (Design, Graphic, Photos, Video, Draw)", category: "Create", path: "/docs#creative-suite" },
  { id: "infra-ops", title: "Infrastructure & Node Operations (Cloud, Edge, Vault)", category: "Operate", path: "/docs#infra-ops" },
  { id: "api-reference", title: "Systems-API & OpenAPI Contract Parity", category: "API", path: "/docs#api-reference" },
];

export default function CommandPalette({ open, onClose, apps, user }: CommandPaletteProps) {
  const [query, setQuery] = useState("");
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);
  const navigate = useNavigate();
  const [, setTheme] = useTheme();
  const [, setDensity] = useDensity();

  // Reset state when opening
  useEffect(() => {
    if (open) {
      setQuery("");
      setSelectedIndex(0);
      setTimeout(() => inputRef.current?.focus(), 50);
    }
  }, [open]);

  // Build searchable items
  const items = useMemo(() => {
    const list: SearchItem[] = [];

    // 1. Applications
    for (const a of apps) {
      list.push({
        id: `app-${a.id}`,
        type: "app",
        title: a.name,
        subtitle: a.description || `${a.path} (${a.primaryShelf ?? "apps"})`,
        category: (a.primaryShelf ?? "apps").toUpperCase(),
        badge: a.health === "healthy" ? "● Online" : "○ Offline",
        action: () => {
          onClose();
          if (a.delivery === "external" && a.publicUrl) {
            window.location.href = a.publicUrl;
          } else {
            navigate(a.path);
          }
        },
      });
    }

    // 2. Documentation Sections
    for (const d of DOC_TOPICS) {
      list.push({
        id: `doc-${d.id}`,
        type: "doc",
        title: d.title,
        subtitle: `Documentation • ${d.category}`,
        category: "DOCS",
        action: () => {
          onClose();
          navigate(d.path);
        },
      });
    }

    // 3. Quick Actions
    list.push({
      id: "act-theme-void",
      type: "action",
      title: "Set Theme: Void (High-Contrast Dark)",
      subtitle: "Switch dashboard and design system theme to Void",
      category: "ACTION",
      action: () => {
        setTheme("void");
        onClose();
      },
    });

    list.push({
      id: "act-theme-slate",
      type: "action",
      title: "Set Theme: Slate (Deep Neutral)",
      subtitle: "Switch theme to Slate",
      category: "ACTION",
      action: () => {
        setTheme("slate");
        onClose();
      },
    });

    list.push({
      id: "act-density-compact",
      type: "action",
      title: "Toggle Density: Compact Scale",
      subtitle: "Maximize screen information density",
      category: "ACTION",
      action: () => {
        setDensity("compact");
        onClose();
      },
    });

    list.push({
      id: "act-density-balanced",
      type: "action",
      title: "Toggle Density: Balanced Scale",
      subtitle: "Standard comfortable UI padding",
      category: "ACTION",
      action: () => {
        setDensity("balanced");
        onClose();
      },
    });

    list.push({
      id: "act-account",
      type: "action",
      title: "Account Settings & Sessions",
      subtitle: "Manage security, passwords, and sessions",
      category: "ACTION",
      action: () => {
        onClose();
        navigate("/account");
      },
    });

    if (user?.role === "founder" || user?.role === "admin") {
      list.push({
        id: "act-admin",
        type: "action",
        title: "Operator Console (Admin)",
        subtitle: "Review access requests, invite tokens, and node health",
        category: "ACTION",
        action: () => {
          onClose();
          navigate("/admin");
        },
      });
    }

    list.push({
      id: "act-logout",
      type: "action",
      title: "Log Out of Platform",
      subtitle: "Revoke active session and sign out",
      category: "ACTION",
      action: () => {
        onClose();
        void logout();
      },
    });

    // Filter by query
    const q = query.trim().toLowerCase();
    if (!q) return list;

    return list.filter(
      (item) =>
        item.title.toLowerCase().includes(q) ||
        item.subtitle.toLowerCase().includes(q) ||
        (item.category && item.category.toLowerCase().includes(q))
    );
  }, [apps, user, query, navigate, onClose, setTheme, setDensity]);

  // Keep selected index in bounds
  useEffect(() => {
    setSelectedIndex((prev) => Math.min(prev, Math.max(0, items.length - 1)));
  }, [items.length]);

  // Keyboard navigation inside palette
  useEffect(() => {
    if (!open) return;

    function handleKeyDown(e: KeyboardEvent) {
      if (e.key === "Escape") {
        e.preventDefault();
        onClose();
      } else if (e.key === "ArrowDown") {
        e.preventDefault();
        setSelectedIndex((prev) => (prev + 1) % Math.max(1, items.length));
      } else if (e.key === "ArrowUp") {
        e.preventDefault();
        setSelectedIndex((prev) => (prev - 1 + items.length) % Math.max(1, items.length));
      } else if (e.key === "Enter") {
        e.preventDefault();
        if (items[selectedIndex]) {
          items[selectedIndex].action();
        }
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [open, items, selectedIndex, onClose]);

  // Auto scroll into view
  useEffect(() => {
    const activeEl = listRef.current?.children[selectedIndex] as HTMLElement | undefined;
    if (activeEl && listRef.current) {
      const containerTop = listRef.current.scrollTop;
      const containerBottom = containerTop + listRef.current.clientHeight;
      const elTop = activeEl.offsetTop;
      const elBottom = elTop + activeEl.clientHeight;

      if (elTop < containerTop) {
        listRef.current.scrollTop = elTop;
      } else if (elBottom > containerBottom) {
        listRef.current.scrollTop = elBottom - listRef.current.clientHeight;
      }
    }
  }, [selectedIndex]);

  if (!open) return null;

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label="Command Palette and Global Search"
      className="fixed inset-0 z-50 flex items-start justify-center pt-16 sm:pt-24 px-4 bg-black/75 backdrop-blur-sm"
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="w-full max-w-2xl overflow-hidden rounded-xl border border-zinc-800 bg-zinc-950 shadow-2xl ring-1 ring-white/10 text-zinc-200">
        {/* Search Input Bar */}
        <div className="flex items-center border-b border-zinc-800 px-4 py-3">
          <svg
            className="h-5 w-5 text-zinc-400 shrink-0 mr-3"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            strokeWidth={2}
          >
            <path strokeLinecap="round" strokeLinejoin="round" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
          </svg>
          <input
            ref={inputRef}
            type="text"
            value={query}
            onChange={(e) => {
              setQuery(e.target.value);
              setSelectedIndex(0);
            }}
            placeholder="Search all 26 apps, guides, settings, or actions... (Esc to close)"
            className="w-full bg-transparent font-sans text-sm sm:text-base text-zinc-100 placeholder-zinc-500 focus:outline-none"
          />
          {query && (
            <button
              type="button"
              onClick={() => {
                setQuery("");
                inputRef.current?.focus();
              }}
              className="text-xs text-zinc-500 hover:text-zinc-300 ml-2"
            >
              Clear
            </button>
          )}
        </div>

        {/* Results List */}
        <div ref={listRef} className="max-h-96 overflow-y-auto p-2 divide-y divide-zinc-900">
          {items.length === 0 ? (
            <div className="py-12 text-center text-sm text-zinc-500">
              No applications or guides found for &ldquo;{query}&rdquo;
            </div>
          ) : (
            items.map((item, idx) => {
              const isSelected = idx === selectedIndex;
              return (
                <div
                  key={item.id}
                  onClick={() => item.action()}
                  onMouseEnter={() => setSelectedIndex(idx)}
                  className={`flex items-center justify-between gap-3 px-3 py-2.5 rounded-lg cursor-pointer transition-colors ${
                    isSelected ? "bg-accent/15 text-white" : "hover:bg-zinc-900 text-zinc-300"
                  }`}
                >
                  <div className="flex items-center gap-3 min-w-0">
                    <span
                      className={`flex h-8 w-8 shrink-0 items-center justify-center rounded-md font-mono text-xs font-semibold uppercase ${
                        item.type === "app"
                          ? "bg-accent/10 text-accent border border-accent/30"
                          : item.type === "doc"
                          ? "bg-sky-950/60 text-sky-400 border border-sky-800/40"
                          : "bg-zinc-800 text-zinc-300 border border-zinc-700"
                      }`}
                    >
                      {item.type === "app" ? "APP" : item.type === "doc" ? "DOC" : "ACT"}
                    </span>
                    <div className="min-w-0">
                      <div className="flex items-center gap-2">
                        <span className="font-medium text-sm text-zinc-100 truncate">{item.title}</span>
                        {item.category && (
                          <span className="font-mono text-[10px] tracking-wider text-zinc-500 uppercase">
                            {item.category}
                          </span>
                        )}
                      </div>
                      <p className="text-xs text-zinc-400 truncate">{item.subtitle}</p>
                    </div>
                  </div>

                  <div className="shrink-0 flex items-center gap-2">
                    {item.badge && (
                      <span className="font-mono text-[10px] text-zinc-500">{item.badge}</span>
                    )}
                    {isSelected && (
                      <kbd className="hidden sm:inline-block rounded border border-zinc-700 bg-zinc-800 px-1.5 py-0.5 font-mono text-[10px] text-zinc-300">
                        ↵ Enter
                      </kbd>
                    )}
                  </div>
                </div>
              );
            })
          )}
        </div>

        {/* Footer shortcuts helper */}
        <div className="flex items-center justify-between border-t border-zinc-800/80 bg-zinc-900/60 px-4 py-2 font-mono text-[11px] text-zinc-500">
          <div className="flex items-center gap-3">
            <span>
              <kbd className="rounded border border-zinc-700 bg-zinc-800 px-1 text-[10px]">↑</kbd>{" "}
              <kbd className="rounded border border-zinc-700 bg-zinc-800 px-1 text-[10px]">↓</kbd> navigate
            </span>
            <span>
              <kbd className="rounded border border-zinc-700 bg-zinc-800 px-1 text-[10px]">↵</kbd> select
            </span>
            <span>
              <kbd className="rounded border border-zinc-700 bg-zinc-800 px-1 text-[10px]">Esc</kbd> dismiss
            </span>
          </div>
          <span>Nexus Global Search</span>
        </div>
      </div>
    </div>
  );
}
