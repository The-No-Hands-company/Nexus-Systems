import React, { useState, useEffect, useCallback, useMemo, useRef } from "react";

// ── Types ──
interface Page {
  slug: string;
  title: string;
  content: string;
  category?: string | null;
  tags?: string[];
  version: number;
  created_at: string;
  updated_at: string;
}

interface PageSummary {
  slug: string;
  title: string;
  updated_at: string;
  category?: string | null;
}

interface Revision {
  slug: string;
  version: number;
  title: string;
  content: string;
  editor_id: string;
  change_summary: string;
  created_at: string;
}

interface Category {
  name: string;
  count: number;
}

interface Backlink {
  slug: string;
  title: string;
}

interface AuditData {
  orphans: { slug: string; title: string }[];
  broken_links: {
    source_slug: string;
    source_title: string;
    target_slug: string;
    target_title: string;
  }[];
  total_pages: number;
}

interface TocItem {
  id: string;
  text: string;
  level: number;
}

const API = "/api/v1";
const fj = (u: string, o?: RequestInit) =>
  fetch(u, o).then((r) => (r.ok ? r.json() : Promise.reject(r.status)));

// ── Fast Wiki Markdown & TOC Parser ──
function parseWiki(text: string): { html: string; toc: TocItem[]; linkedSlugs: string[] } {
  const toc: TocItem[] = [];
  const linkedSlugs: string[] = [];
  let html = text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");

  // [[Article Name]] or [[Article Name|display text]]
  html = html.replace(/\[\[([^\]|]+)(?:\|([^\]]+))?\]\]/g, (_, slug, display) => {
    const s = slug.trim().toLowerCase().replace(/\s+/g, "-");
    linkedSlugs.push(s);
    return `<a href="#" onclick="event.preventDefault();window.__nav__('${s}')" class="text-indigo-400 hover:text-indigo-300 underline underline-offset-2 wiki-link font-medium">${display || slug}</a>`;
  });

  // Wiki style headings: == Heading ==
  html = html.replace(/^(={1,3})\s*(.+?)\s*\1$/gm, (_, hashes, text) => {
    const level = hashes.length;
    const id = text.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
    toc.push({ id, text, level });
    const tag = `h${level + 1}`;
    return `<${tag} id="${id}" class="font-bold mt-7 mb-3 scroll-mt-20 ${
      level === 1
        ? "text-2xl border-b border-slate-800 pb-2 text-white"
        : level === 2
        ? "text-xl text-slate-100"
        : "text-lg text-slate-200"
    }">${text}</${tag}>`;
  });

  // Markdown style headings: # Heading
  html = html.replace(/^(#{1,3})\s+(.+)$/gm, (_, hashes, text) => {
    const level = hashes.length;
    const id = text.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
    toc.push({ id, text, level });
    const tag = `h${level + 1}`;
    return `<${tag} id="${id}" class="font-bold mt-7 mb-3 scroll-mt-20 ${
      level === 1
        ? "text-2xl border-b border-slate-800 pb-2 text-white"
        : level === 2
        ? "text-xl text-slate-100"
        : "text-lg text-slate-200"
    }">${text}</${tag}>`;
  });

  // Code blocks
  html = html.replace(/```([a-z0-9_-]*)\n([\s\S]*?)```/g, (_, lang, code) => {
    return `<pre class="bg-slate-900 border border-slate-800 p-4 rounded-xl overflow-x-auto text-sm my-4 font-mono text-indigo-300"><code>${code}</code></pre>`;
  });

  // Inline code, bold, italic, quotes, lists
  html = html.replace(/`([^`]+)`/g, "<code class='bg-slate-800 text-indigo-300 px-1.5 py-0.5 rounded text-xs font-mono'>$1</code>");
  html = html.replace(/\*\*(.+?)\*\*/g, "<strong class='font-bold text-white'>$1</strong>");
  html = html.replace(/(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)/g, "<em class='italic text-slate-300'>$1</em>");
  html = html.replace(/^(\*|-)\s+(.+)$/gm, "<li class='ml-5 list-disc text-slate-300 my-1'>$2</li>");
  html = html.replace(/^>\s+(.+)$/gm, "<blockquote class='border-l-4 border-indigo-500 pl-4 py-1 italic text-slate-300 my-3 bg-slate-900/40 rounded-r'>$1</blockquote>");
  html = html.replace(/\n\n+/g, "</p><p class='my-3 leading-relaxed text-slate-300'>");
  html = "<p class='my-3 leading-relaxed text-slate-300'>" + html + "</p>";

  return { html, toc, linkedSlugs };
}

// ── Line Diff ──
function lineDiff(oldLines: string[], newLines: string[]): string {
  let result = "";
  const max = Math.max(oldLines.length, newLines.length);
  for (let i = 0; i < max; i++) {
    if (i >= oldLines.length) {
      result += `<div class="bg-emerald-950/40 text-emerald-400 px-2 py-0.5 border-l-2 border-emerald-500">+ ${newLines[i]}</div>`;
    } else if (i >= newLines.length) {
      result += `<div class="bg-rose-950/40 text-rose-400 px-2 py-0.5 border-l-2 border-rose-500">- ${oldLines[i]}</div>`;
    } else if (oldLines[i] !== newLines[i]) {
      result += `<div class="bg-rose-950/30 text-rose-400 px-2 py-0.5 border-l-2 border-rose-500">- ${oldLines[i]}</div>`;
      result += `<div class="bg-emerald-950/30 text-emerald-400 px-2 py-0.5 border-l-2 border-emerald-500">+ ${newLines[i]}</div>`;
    } else {
      result += `<div class="text-slate-400 px-2 py-0.5">  ${oldLines[i]}</div>`;
    }
  }
  return result;
}

// ── Main App Component ──
export default function App() {
  const [view, setView] = useState<"browse" | "read" | "edit" | "history" | "diff" | "talk" | "links" | "audit">("browse");
  const [slug, setSlug] = useState("");
  const [pages, setPages] = useState<PageSummary[]>([]);
  const [page, setPage] = useState<Page | null>(null);
  const [revisions, setRevisions] = useState<Revision[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [backlinks, setBacklinks] = useState<Backlink[]>([]);
  const [talkPage, setTalkPage] = useState<Page | null>(null);
  const [diffData, setDiffData] = useState<{ v1: number; v2: number; diff: string[] } | null>(null);
  const [search, setSearch] = useState("");
  const [selectedCat, setSelectedCat] = useState("");
  const [user, setUser] = useState<{ user_id: string; username: string } | null>(null);
  const [authUser, setAuthUser] = useState("");
  const [authPass, setAuthPass] = useState("");

  // Audit state
  const [auditData, setAuditData] = useState<AuditData | null>(null);
  const [loadingAudit, setLoadingAudit] = useState(false);

  // Cmd+K Search Modal
  const [showSearchModal, setShowSearchModal] = useState(false);
  const [modalQuery, setModalQuery] = useState("");
  const [modalResults, setModalResults] = useState<PageSummary[]>([]);
  const [selectedIndex, setSelectedIndex] = useState(0);

  // Drag and drop state for category reordering
  const [draggedSlug, setDraggedSlug] = useState<string | null>(null);

  const loadPages = useCallback(async () => {
    try {
      setPages((await fj(`${API}/pages`)).items);
    } catch {}
  }, []);

  const loadCategories = useCallback(async () => {
    try {
      setCategories((await fj(`${API}/categories`)).categories);
    } catch {}
  }, []);

  useEffect(() => {
    loadPages();
    loadCategories();
  }, [loadPages, loadCategories]);

  // Hook global navigation
  useEffect(() => {
    (window as any).__nav__ = (s: string) => openPage(s);
  }, []);

  // Cmd+K Keyboard Shortcut
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "k") {
        e.preventDefault();
        setShowSearchModal((prev) => !prev);
      }
    };
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, []);

  // Modal search debouncing
  useEffect(() => {
    if (!modalQuery.trim()) {
      setModalResults(pages.slice(0, 10));
      setSelectedIndex(0);
      return;
    }
    const timer = setTimeout(async () => {
      try {
        const res = await fj(`${API}/search?q=${encodeURIComponent(modalQuery.trim())}`);
        setModalResults(res.items || []);
        setSelectedIndex(0);
      } catch {
        setModalResults([]);
      }
    }, 150);
    return () => clearTimeout(timer);
  }, [modalQuery, pages]);

  const openPage = async (s: string) => {
    try {
      const p = await fj(`${API}/pages/${s}`);
      setPage(p);
      setSlug(s);
      setView("read");
      setShowSearchModal(false);
    } catch {
      // If page doesn't exist, offer to create it
      setSlug(s);
      setPage({
        slug: s,
        title: s.replace(/-/g, " ").replace(/\b\w/g, (c) => c.toUpperCase()),
        content: `# ${s.replace(/-/g, " ").replace(/\b\w/g, (c) => c.toUpperCase())}\n\nStart writing this article...`,
        category: selectedCat || "General",
        tags: [],
        version: 0,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      });
      setView("edit");
      setShowSearchModal(false);
    }
  };

  const openEdit = (s: string) => {
    setSlug(s);
    setView("edit");
  };

  const randomPage = async () => {
    try {
      const p = await fj(`${API}/pages/random`);
      openPage(p.slug);
    } catch {}
  };

  const openHistory = async (s: string) => {
    try {
      setRevisions((await fj(`${API}/pages/${s}/revisions`)).items);
      setSlug(s);
      setView("history");
    } catch {}
  };

  const openDiff = async (s: string, v1: number, v2: number) => {
    try {
      setDiffData(await fj(`${API}/diff/${s}?v1=${v1}&v2=${v2}`));
      setSlug(s);
      setView("diff");
    } catch {}
  };

  const openTalk = async (s: string) => {
    try {
      setTalkPage(await fj(`${API}/pages/${s}/talk`));
      setSlug(s);
      setView("talk");
    } catch {}
  };

  const openLinks = async (s: string) => {
    try {
      setBacklinks((await fj(`${API}/pages/${s}/links`)).backlinks);
      setSlug(s);
      setView("links");
    } catch {}
  };

  const openAudit = async () => {
    setView("audit");
    setLoadingAudit(true);
    try {
      const res = await fj(`${API}/audit`);
      setAuditData(res);
    } catch (e) {
      console.error("Failed to load audit", e);
    } finally {
      setLoadingAudit(false);
    }
  };

  const goBrowse = () => {
    setView("browse");
    setSearch("");
    setSelectedCat("");
  };

  const login = async () => {
    try {
      const u = await fj(`${API}/auth/login`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ username: authUser, password: authPass }),
      });
      setUser(u);
    } catch {}
  };

  const register = async () => {
    try {
      const u = await fj(`${API}/auth/register`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ username: authUser, password: authPass }),
      });
      setUser(u);
    } catch {}
  };

  // Move page to category via drag & drop
  const handleDropOnCategory = async (targetCategory: string) => {
    if (!draggedSlug) return;
    try {
      await fj(`${API}/pages/${draggedSlug}`, {
        method: "PATCH",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ category: targetCategory }),
      });
      loadPages();
      loadCategories();
      if (page && page.slug === draggedSlug) {
        setPage({ ...page, category: targetCategory });
      }
    } catch (err) {
      console.error("Failed to reorder category", err);
    } finally {
      setDraggedSlug(null);
    }
  };

  const handleRollback = async (pageSlug: string, version: number) => {
    if (!confirm(`Are you sure you want to rollback this article to version ${version}?`)) return;
    try {
      const restored = await fj(`${API}/pages/${pageSlug}/rollback/${version}?editor_id=${encodeURIComponent(user?.username || "anonymous")}`, {
        method: "POST",
      });
      setPage(restored);
      setView("read");
      loadPages();
      loadCategories();
    } catch {
      alert("Failed to rollback revision");
    }
  };

  const filtered = pages.filter((p) => {
    if (selectedCat && (p.category || "General").toLowerCase() !== selectedCat.toLowerCase()) return false;
    if (search && !p.title.toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  return (
    <div className="min-h-screen bg-[#090d16] text-slate-100 font-sans flex flex-col antialiased selection:bg-indigo-600/30 selection:text-indigo-200">
      {/* ── Top Header ── */}
      <header className="border-b border-slate-800 bg-[#0c1222]/90 backdrop-blur sticky top-0 z-30">
        <div className="max-w-7xl mx-auto px-4 py-3 flex items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <button
              onClick={goBrowse}
              className="flex items-center gap-2 text-lg font-bold tracking-tight text-white hover:text-indigo-400 transition-colors"
            >
              <div className="w-7 h-7 rounded-lg bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400 text-xs font-mono">
                W
              </div>
              <span>Nexus Wiki</span>
            </button>
            <span className="text-[10px] bg-slate-800 text-slate-400 px-2 py-0.5 rounded-full font-mono border border-slate-700">
              v1.0
            </span>
          </div>

          {/* Search Trigger */}
          <div className="flex-1 max-w-md mx-4">
            <button
              onClick={() => setShowSearchModal(true)}
              className="w-full bg-[#131b2e] hover:bg-[#1a243d] border border-slate-800 hover:border-slate-700 rounded-xl px-3.5 py-1.5 text-xs text-slate-400 flex items-center justify-between transition-colors shadow-inner"
            >
              <div className="flex items-center gap-2">
                <svg className="w-3.5 h-3.5 text-slate-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
                </svg>
                <span>Search articles, headings, concepts...</span>
              </div>
              <kbd className="bg-slate-800 border border-slate-700 text-slate-400 px-1.5 py-0.5 rounded text-[10px] font-mono shadow-sm">
                ⌘K
              </kbd>
            </button>
          </div>

          {/* Quick Actions */}
          <div className="flex items-center gap-2">
            <button
              onClick={openAudit}
              className={`px-3 py-1.5 text-xs rounded-lg transition-colors border ${
                view === "audit"
                  ? "bg-indigo-600 text-white border-indigo-500"
                  : "bg-slate-900 border-slate-800 text-slate-300 hover:text-white hover:bg-slate-800"
              }`}
              title="Wiki Health & Dead Link Audit"
            >
              Health Audit
            </button>
            <button
              onClick={randomPage}
              className="px-3 py-1.5 text-xs bg-slate-900 border border-slate-800 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800 transition-colors"
            >
              Random
            </button>
            <button
              onClick={() => openEdit("")}
              className="px-3 py-1.5 text-xs bg-indigo-600 hover:bg-indigo-500 text-white font-medium rounded-lg transition-colors shadow-sm"
            >
              + New Article
            </button>

            {user ? (
              <span className="text-xs text-slate-400 font-mono ml-2">{user.username}</span>
            ) : (
              <div className="flex gap-1 ml-2">
                <input
                  value={authUser}
                  onChange={(e) => setAuthUser(e.target.value)}
                  placeholder="user"
                  className="w-16 bg-slate-900 px-2 py-1 rounded text-xs border border-slate-800 text-slate-200"
                />
                <input
                  value={authPass}
                  onChange={(e) => setAuthPass(e.target.value)}
                  type="password"
                  placeholder="pass"
                  className="w-16 bg-slate-900 px-2 py-1 rounded text-xs border border-slate-800 text-slate-200"
                />
                <button
                  onClick={login}
                  className="px-2 py-1 text-xs bg-slate-800 hover:bg-slate-700 rounded border border-slate-700"
                >
                  Login
                </button>
                <button
                  onClick={register}
                  className="px-2 py-1 text-xs bg-indigo-950/60 hover:bg-indigo-900 text-indigo-300 rounded border border-indigo-800/40"
                >
                  Reg
                </button>
              </div>
            )}
          </div>
        </div>
      </header>

      {/* ── Main Workspace ── */}
      <main className="max-w-7xl mx-auto px-4 py-6 flex-1 w-full flex gap-6">
        {/* Left Sidebar: Categories & Tree */}
        <aside className="w-60 shrink-0 flex flex-col gap-4">
          <div className="bg-[#0f172a]/70 border border-slate-800 rounded-2xl p-3 shadow-lg">
            <div className="flex items-center justify-between px-2 mb-2">
              <h3 className="text-[11px] uppercase tracking-wider text-slate-400 font-semibold">
                Categories
              </h3>
              <span className="text-[10px] text-slate-500 font-mono">{categories.length}</span>
            </div>

            <div className="space-y-1">
              <button
                onClick={() => {
                  setSelectedCat("");
                  if (view !== "browse") setView("browse");
                }}
                onDragOver={(e) => e.preventDefault()}
                onDrop={() => handleDropOnCategory("General")}
                className={`w-full text-left text-xs px-3 py-1.5 rounded-lg transition-colors flex items-center justify-between ${
                  !selectedCat && view === "browse"
                    ? "bg-indigo-600/20 text-indigo-300 font-medium border border-indigo-500/30"
                    : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/60"
                }`}
              >
                <span>All Articles</span>
                <span className="text-[10px] bg-slate-800 px-1.5 py-0.5 rounded text-slate-400 font-mono">
                  {pages.length}
                </span>
              </button>

              {categories.map((c) => (
                <button
                  key={c.name}
                  onClick={() => {
                    setSelectedCat(c.name);
                    if (view !== "browse") setView("browse");
                  }}
                  onDragOver={(e) => e.preventDefault()}
                  onDrop={() => handleDropOnCategory(c.name)}
                  className={`w-full text-left text-xs px-3 py-1.5 rounded-lg transition-colors flex items-center justify-between group ${
                    selectedCat === c.name && view === "browse"
                      ? "bg-indigo-600/20 text-indigo-300 font-medium border border-indigo-500/30"
                      : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/60"
                  }`}
                  title="Drop an article here to categorize"
                >
                  <span className="truncate">{c.name}</span>
                  <span className="text-[10px] bg-slate-800 px-1.5 py-0.5 rounded text-slate-500 group-hover:text-slate-300 font-mono">
                    {c.count}
                  </span>
                </button>
              ))}
            </div>
          </div>

          {/* Quick Page Tree */}
          <div className="bg-[#0f172a]/70 border border-slate-800 rounded-2xl p-3 shadow-lg flex-1">
            <h3 className="text-[11px] uppercase tracking-wider text-slate-400 font-semibold px-2 mb-2">
              Recent Articles
            </h3>
            <div className="space-y-0.5 max-h-96 overflow-y-auto">
              {pages.slice(0, 15).map((p) => (
                <div
                  key={p.slug}
                  draggable
                  onDragStart={() => setDraggedSlug(p.slug)}
                  onClick={() => openPage(p.slug)}
                  className={`px-2.5 py-1.5 rounded-lg text-xs cursor-pointer truncate transition-colors flex items-center gap-1.5 ${
                    slug === p.slug && view === "read"
                      ? "bg-indigo-600/20 text-indigo-300 border border-indigo-500/30 font-medium"
                      : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/50"
                  }`}
                >
                  <span className="text-slate-500">•</span>
                  <span className="truncate">{p.title}</span>
                </div>
              ))}
            </div>
          </div>
        </aside>

        {/* ── Center Content Area ── */}
        <section className="flex-1 min-w-0">
          {/* BROWSE VIEW */}
          {view === "browse" && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6">
              <div className="flex items-center justify-between mb-4 pb-3 border-b border-slate-800">
                <h2 className="text-base font-semibold text-white">
                  {selectedCat ? `Category: ${selectedCat}` : search ? `Search: "${search}"` : "All Wiki Articles"}
                </h2>
                <span className="text-xs text-slate-500 font-mono">{filtered.length} articles</span>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-2.5">
                {filtered.map((p) => (
                  <div
                    key={p.slug}
                    draggable
                    onDragStart={() => setDraggedSlug(p.slug)}
                    onClick={() => openPage(p.slug)}
                    className="p-3.5 rounded-xl border border-slate-800 hover:border-slate-700 bg-[#0f172a]/80 hover:bg-[#131b2e] cursor-pointer transition-colors group flex flex-col justify-between"
                  >
                    <div>
                      <div className="flex items-start justify-between gap-2">
                        <h4 className="text-sm font-semibold text-slate-200 group-hover:text-indigo-400 transition-colors">
                          {p.title}
                        </h4>
                        <span className="text-[10px] bg-slate-800/80 text-slate-400 px-1.5 py-0.5 rounded border border-slate-700/60 font-mono flex-shrink-0">
                          {p.category || "General"}
                        </span>
                      </div>
                      <p className="text-[11px] text-slate-500 font-mono mt-1">
                        slug: /{p.slug}
                      </p>
                    </div>
                    <div className="flex items-center justify-between text-[11px] text-slate-500 mt-3 pt-2 border-t border-slate-800/60 font-mono">
                      <span>{new Date(p.updated_at).toLocaleDateString()}</span>
                      <span className="text-slate-400 group-hover:text-indigo-300">Read article →</span>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* READ VIEW */}
          {view === "read" && page && (
            <div className="flex gap-6">
              {/* Article Main Text */}
              <div className="flex-1 bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-8 min-w-0">
                <Toolbar
                  slug={slug}
                  onEdit={openEdit}
                  onHistory={openHistory}
                  onTalk={openTalk}
                  onLinks={openLinks}
                  onBrowse={goBrowse}
                />

                <h1 className="text-3xl font-extrabold text-white mt-4 mb-2 tracking-tight">
                  {page.title}
                </h1>

                <div className="flex items-center gap-2 mb-6 text-xs text-slate-500 font-mono">
                  {page.category && (
                    <span className="bg-slate-800 border border-slate-700 text-slate-300 px-2 py-0.5 rounded-md">
                      {page.category}
                    </span>
                  )}
                  {page.tags?.map((t) => (
                    <span key={t} className="text-indigo-400">#{t}</span>
                  ))}
                  <span>•</span>
                  <span>Version {page.version}</span>
                  <span>•</span>
                  <span>Updated {new Date(page.updated_at).toLocaleDateString()}</span>
                </div>

                {/* Article Prose Content */}
                {(() => {
                  const { html } = parseWiki(page.content);
                  return (
                    <div
                      className="prose prose-invert max-w-none text-slate-300 leading-relaxed text-sm"
                      dangerouslySetInnerHTML={{ __html: html }}
                    />
                  );
                })()}
              </div>

              {/* Sticky Right-Hand Table of Contents */}
              {(() => {
                const { toc } = parseWiki(page.content);
                return (
                  <aside className="w-56 shrink-0 hidden lg:block sticky top-20 self-start">
                    <div className="bg-[#0f172a]/70 border border-slate-800 rounded-2xl p-4 shadow-lg">
                      <h4 className="text-[11px] uppercase tracking-wider font-semibold text-slate-400 mb-3">
                        On This Page
                      </h4>
                      {toc.length === 0 ? (
                        <p className="text-xs text-slate-600 italic">No headings found.</p>
                      ) : (
                        <ul className="space-y-1.5 text-xs">
                          {toc.map((t) => (
                            <li
                              key={t.id}
                              style={{ paddingLeft: `${(t.level - 1) * 10}px` }}
                            >
                              <a
                                href={`#${t.id}`}
                                onClick={(e) => {
                                  e.preventDefault();
                                  document.getElementById(t.id)?.scrollIntoView({ behavior: "smooth" });
                                }}
                                className="text-slate-400 hover:text-indigo-300 transition-colors block truncate"
                              >
                                {t.text}
                              </a>
                            </li>
                          ))}
                        </ul>
                      )}
                    </div>
                  </aside>
                );
              })()}
            </div>
          )}

          {/* EDIT VIEW */}
          {view === "edit" && (
            <Editor
              slug={slug}
              page={page}
              user={user}
              onSave={(s: string) => openPage(s)}
              onCancel={() => (slug ? openPage(slug) : goBrowse())}
            />
          )}

          {/* REVISION HISTORY VIEW */}
          {view === "history" && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6">
              <button
                onClick={() => slug && openPage(slug)}
                className="text-xs text-slate-400 hover:text-white mb-4 block font-mono"
              >
                ← Back to Article
              </button>
              <h2 className="text-lg font-bold text-white mb-4">
                Revision History — {page?.title}
              </h2>
              <div className="space-y-2">
                {revisions.map((r, i) => (
                  <div
                    key={r.version}
                    className="p-3.5 border border-slate-800 bg-[#0f172a]/80 rounded-xl flex items-center justify-between"
                  >
                    <div>
                      <span className="font-semibold text-xs text-white">v{r.version}</span>
                      <span className="text-xs text-slate-500 font-mono ml-2">
                        {new Date(r.created_at).toLocaleString()}
                      </span>
                      <span className="text-xs text-slate-400 ml-2">
                        — {r.editor_id}: {r.change_summary}
                      </span>
                    </div>
                    <div className="flex items-center gap-2">
                      {i > 0 && (
                        <button
                          onClick={() => handleRollback(slug, r.version)}
                          className="text-xs px-2.5 py-1 bg-amber-500/15 hover:bg-amber-500/30 text-amber-300 rounded-lg border border-amber-500/30 font-medium transition-colors"
                          title={`Rollback article to v${r.version}`}
                        >
                          Rollback to v{r.version}
                        </button>
                      )}
                      {i < revisions.length - 1 && (
                        <button
                          onClick={() => openDiff(slug, r.version, revisions[i + 1].version)}
                          className="text-xs px-2.5 py-1 bg-slate-800 hover:bg-slate-700 rounded-lg border border-slate-700 text-slate-300"
                        >
                          Diff with v{revisions[i + 1].version}
                        </button>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* DIFF VIEW */}
          {view === "diff" && diffData && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6">
              <button
                onClick={() => slug && openHistory(slug)}
                className="text-xs text-slate-400 hover:text-white mb-4 block font-mono"
              >
                ← Back to History
              </button>
              <h2 className="text-lg font-bold text-white mb-2">
                Changes: v{diffData.v2} → v{diffData.v1}
              </h2>
              <div
                className="bg-slate-950 border border-slate-800 rounded-xl p-4 font-mono text-xs overflow-x-auto shadow-inner"
                style={{ maxHeight: "70vh" }}
              >
                <div
                  dangerouslySetInnerHTML={{
                    __html: lineDiff(
                      (revisions.find((r) => r.version === diffData.v2)?.content || "").split("\n"),
                      (revisions.find((r) => r.version === diffData.v1)?.content || "").split("\n")
                    ),
                  }}
                />
              </div>
            </div>
          )}

          {/* TALK VIEW */}
          {view === "talk" && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6">
              <button
                onClick={() => slug && openPage(slug)}
                className="text-xs text-slate-400 hover:text-white mb-4 block font-mono"
              >
                ← Back to Article
              </button>
              <h2 className="text-lg font-bold text-white mb-4">Talk: {page?.title}</h2>
              {talkPage && (
                <div
                  className="prose prose-invert max-w-none text-sm text-slate-300"
                  dangerouslySetInnerHTML={{ __html: parseWiki(talkPage.content).html }}
                />
              )}
              <div className="mt-6 p-4 bg-[#0f172a] border border-slate-800 rounded-xl">
                <h3 className="text-xs font-semibold text-slate-200 uppercase tracking-wider mb-2">
                  New Discussion
                </h3>
                <TalkEditor slug={slug} user={user} onSaved={() => openTalk(slug)} />
              </div>
            </div>
          )}

          {/* WHAT LINKS HERE VIEW */}
          {view === "links" && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6">
              <button
                onClick={() => slug && openPage(slug)}
                className="text-xs text-slate-400 hover:text-white mb-4 block font-mono"
              >
                ← Back to Article
              </button>
              <h2 className="text-lg font-bold text-white mb-4">
                What links to: {page?.title}
              </h2>
              {backlinks.length === 0 ? (
                <p className="text-slate-500 text-sm">No incoming backlinks found yet.</p>
              ) : (
                <ul className="space-y-1.5">
                  {backlinks.map((b) => (
                    <li key={b.slug}>
                      <button
                        onClick={() => openPage(b.slug)}
                        className="text-indigo-400 hover:underline text-sm font-medium"
                      >
                        {b.title}
                      </button>
                    </li>
                  ))}
                </ul>
              )}
            </div>
          )}

          {/* HEALTH AUDIT VIEW */}
          {view === "audit" && (
            <div className="bg-[#0f172a]/40 border border-slate-800/80 rounded-2xl p-6 flex flex-col gap-6">
              <div className="flex items-center justify-between pb-3 border-b border-slate-800">
                <div>
                  <h2 className="text-lg font-bold text-white">Wiki Health & Cross-Reference Audit</h2>
                  <p className="text-xs text-slate-400">
                    Audit of broken links, missing stub articles, and orphaned pages.
                  </p>
                </div>
                <button
                  onClick={openAudit}
                  className="px-3 py-1.5 text-xs bg-slate-800 hover:bg-slate-700 rounded-lg text-slate-300 font-mono"
                >
                  Re-audit
                </button>
              </div>

              {loadingAudit ? (
                <div className="p-8 text-center text-xs text-slate-500 font-mono">
                  Auditing wiki cross-references...
                </div>
              ) : auditData ? (
                <div className="flex flex-col gap-6">
                  {/* Broken Links */}
                  <div>
                    <h3 className="text-sm font-semibold text-rose-400 mb-2 flex items-center gap-2">
                      <span>Dead / Broken Links ({auditData.broken_links.length})</span>
                    </h3>
                    {auditData.broken_links.length === 0 ? (
                      <p className="text-xs text-slate-500 italic">No broken links detected! All links resolve to articles.</p>
                    ) : (
                      <div className="border border-slate-800 rounded-xl overflow-hidden divide-y divide-slate-800/60">
                        {auditData.broken_links.map((b, i) => (
                          <div key={i} className="p-3 bg-[#0f172a]/60 flex items-center justify-between text-xs">
                            <div>
                              <span className="text-slate-400">Referenced in: </span>
                              <button onClick={() => openPage(b.source_slug)} className="text-indigo-400 hover:underline font-medium">
                                {b.source_title}
                              </button>
                              <span className="text-slate-500 mx-2">→</span>
                              <span className="text-rose-400 font-mono">[[{b.target_slug}]]</span>
                            </div>
                            <button
                              onClick={() => openPage(b.target_slug)}
                              className="px-2.5 py-1 bg-indigo-600/30 hover:bg-indigo-600/50 text-indigo-300 rounded text-[11px] font-medium"
                            >
                              Create Stub Article
                            </button>
                          </div>
                        ))}
                      </div>
                    )}
                  </div>

                  {/* Orphaned Articles */}
                  <div>
                    <h3 className="text-sm font-semibold text-amber-400 mb-2 flex items-center gap-2">
                      <span>Orphaned Articles ({auditData.orphans.length})</span>
                    </h3>
                    {auditData.orphans.length === 0 ? (
                      <p className="text-xs text-slate-500 italic">No orphaned articles! Every page is referenced at least once.</p>
                    ) : (
                      <div className="border border-slate-800 rounded-xl overflow-hidden divide-y divide-slate-800/60 max-h-64 overflow-y-auto">
                        {auditData.orphans.map((o) => (
                          <div key={o.slug} className="p-3 bg-[#0f172a]/60 flex items-center justify-between text-xs">
                            <span className="text-slate-300 font-medium">{o.title}</span>
                            <button
                              onClick={() => openPage(o.slug)}
                              className="px-2.5 py-1 bg-slate-800 hover:bg-slate-700 text-slate-300 rounded text-[11px]"
                            >
                              Open Article
                            </button>
                          </div>
                        ))}
                      </div>
                    )}
                  </div>
                </div>
              ) : null}
            </div>
          )}
        </section>
      </main>

      {/* ── Global Cmd+K Search Modal ── */}
      {showSearchModal && (
        <div
          className="fixed inset-0 bg-black/70 backdrop-blur-sm z-50 flex items-start justify-center pt-24 px-4"
          onClick={() => setShowSearchModal(false)}
        >
          <div
            className="w-full max-w-xl bg-[#0f172a] border border-slate-800 rounded-2xl shadow-2xl overflow-hidden flex flex-col"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-3 border-b border-slate-800 flex items-center gap-2.5">
              <svg className="w-4 h-4 text-slate-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
              </svg>
              <input
                type="text"
                autoFocus
                placeholder="Search wiki articles and content..."
                value={modalQuery}
                onChange={(e) => setModalQuery(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === "ArrowDown") {
                    e.preventDefault();
                    setSelectedIndex((prev) => Math.min(prev + 1, modalResults.length - 1));
                  } else if (e.key === "ArrowUp") {
                    e.preventDefault();
                    setSelectedIndex((prev) => Math.max(prev - 1, 0));
                  } else if (e.key === "Enter" && modalResults[selectedIndex]) {
                    e.preventDefault();
                    openPage(modalResults[selectedIndex].slug);
                  } else if (e.key === "Escape") {
                    setShowSearchModal(false);
                  }
                }}
                className="flex-1 bg-transparent text-sm text-white placeholder-slate-500 focus:outline-none"
              />
              <kbd className="text-[10px] bg-slate-800 text-slate-400 px-1.5 py-0.5 rounded font-mono">
                ESC
              </kbd>
            </div>

            <div className="max-h-80 overflow-y-auto divide-y divide-slate-800/50">
              {modalResults.length === 0 ? (
                <div className="p-8 text-center text-xs text-slate-500">
                  No matching articles found. Press Enter to create "{modalQuery}"
                </div>
              ) : (
                modalResults.map((r, i) => (
                  <div
                    key={r.slug}
                    onClick={() => openPage(r.slug)}
                    className={`p-3 cursor-pointer flex items-center justify-between text-xs transition-colors ${
                      i === selectedIndex
                        ? "bg-indigo-600/20 text-white border-l-2 border-indigo-500"
                        : "hover:bg-slate-800/40 text-slate-300"
                    }`}
                  >
                    <div>
                      <div className="font-semibold text-white">{r.title}</div>
                      <div className="text-[10px] text-slate-500 font-mono">/{r.slug}</div>
                    </div>
                    <span className="text-[10px] text-slate-500 font-mono">
                      {new Date(r.updated_at).toLocaleDateString()}
                    </span>
                  </div>
                ))
              )}
            </div>

            <div className="p-2.5 bg-slate-900 border-t border-slate-800/80 text-[10px] text-slate-500 flex items-center justify-between font-mono">
              <span>Navigate with ↑ ↓ and Enter</span>
              <span>Nexus Wiki Sovereign Search</span>
            </div>
          </div>
        </div>
      )}

      {/* Footer */}
      <footer className="border-t border-slate-800/80 py-4 px-6 text-center text-xs text-slate-500 font-mono">
        Nexus Wiki — Sovereign documentation and knowledge base. Powered by Nexus Systems.
      </footer>
    </div>
  );
}

// ── Toolbar ──
function Toolbar({ slug, onEdit, onHistory, onTalk, onLinks, onBrowse }: any) {
  return (
    <div className="flex items-center gap-2 text-xs mb-3 flex-wrap text-slate-400 border-b border-slate-800/60 pb-3">
      <button onClick={onBrowse} className="hover:text-white transition-colors">
        ← Browse
      </button>
      <span className="text-slate-700">|</span>
      <button onClick={() => onEdit(slug)} className="hover:text-indigo-400 transition-colors">
        Edit
      </button>
      <span className="text-slate-700">|</span>
      <button onClick={() => onHistory(slug)} className="hover:text-indigo-400 transition-colors">
        History
      </button>
      <span className="text-slate-700">|</span>
      <button onClick={() => onTalk(slug)} className="hover:text-indigo-400 transition-colors">
        Talk
      </button>
      <span className="text-slate-700">|</span>
      <button onClick={() => onLinks(slug)} className="hover:text-indigo-400 transition-colors">
        What links here
      </button>
    </div>
  );
}

// ── Editor ──
function Editor({ slug, page, user, onSave, onCancel }: any) {
  const [title, setTitle] = useState(page?.title ?? "");
  const [content, setContent] = useState(page?.content ?? "");
  const [category, setCategory] = useState(page?.category ?? "");
  const [saving, setSaving] = useState(false);

  const save = async () => {
    if (!title.trim() || !content.trim()) return;
    setSaving(true);
    try {
      const method = slug && page?.version ? "PATCH" : "POST";
      const url = slug && page?.version ? `${API}/pages/${slug}` : `${API}/pages`;
      const body: any = {
        title: title.trim(),
        content: content.trim(),
        editor_id: user?.username || "anonymous",
        change_summary: "Edited via Web UI",
      };
      if (category.trim()) body.category = category.trim();
      const res = await fetch(url, {
        method,
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
      });
      if (res.ok) {
        const p = await res.json();
        onSave(p.slug);
      }
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="bg-[#0f172a]/60 border border-slate-800/80 rounded-2xl p-6">
      <h2 className="text-base font-semibold text-white mb-4">
        {slug && page?.version ? "Edit" : "Create"} Wiki Article
      </h2>
      <input
        value={title}
        onChange={(e) => setTitle(e.target.value)}
        placeholder="Article Title..."
        className="w-full bg-[#131b2e] px-3.5 py-2 rounded-xl mb-3 border border-slate-800 focus:border-indigo-500 outline-none text-base font-semibold text-white placeholder-slate-500"
      />
      <input
        value={category}
        onChange={(e) => setCategory(e.target.value)}
        placeholder="Category (e.g. Architecture, Operations, Guides)..."
        className="w-full bg-[#131b2e] px-3.5 py-2 rounded-xl mb-3 border border-slate-800 focus:border-indigo-500 outline-none text-xs text-slate-200 placeholder-slate-500"
      />
      <textarea
        value={content}
        onChange={(e) => setContent(e.target.value)}
        placeholder="Write in wiki markdown...&#10;&#10;== Heading ==&#10;[[Linked Page]]&#10;[[Page|display text]]&#10;* list item&#10;> blockquote&#10;`code`"
        rows={20}
        className="w-full bg-[#131b2e] px-4 py-3 rounded-xl mb-4 border border-slate-800 focus:border-indigo-500 outline-none text-xs font-mono leading-relaxed text-slate-200 placeholder-slate-500"
      />
      <div className="flex gap-2">
        <button
          onClick={save}
          disabled={saving}
          className="px-4 py-2 bg-indigo-600 hover:bg-indigo-500 rounded-xl text-xs font-medium text-white disabled:opacity-50 transition-colors shadow-sm"
        >
          {saving ? "Saving..." : "Save Article"}
        </button>
        <button
          onClick={onCancel}
          className="px-4 py-2 bg-slate-800 hover:bg-slate-700 rounded-xl text-xs text-slate-300 transition-colors"
        >
          Cancel
        </button>
      </div>
      <p className="text-[11px] text-slate-500 mt-3 font-mono">
        Syntax: [[links]], == Heading ==, **bold**, *italic*, `code`, - list, &gt; quote
      </p>
    </div>
  );
}

// ── Talk Editor ──
function TalkEditor({ slug, user, onSaved }: { slug: string; user: any; onSaved: () => void }) {
  const [content, setContent] = useState("");
  const save = async () => {
    if (!content.trim()) return;
    await fetch(`${API}/pages/${slug}/talk`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: `Talk: ${slug}`,
        content: content.trim(),
        editor_id: user?.username || "anonymous",
      }),
    });
    setContent("");
    onSaved();
  };
  return (
    <div className="flex gap-2">
      <input
        value={content}
        onChange={(e) => setContent(e.target.value)}
        placeholder="Add a comment to this discussion..."
        className="flex-1 bg-[#131b2e] px-3.5 py-2 rounded-xl text-xs border border-slate-800 focus:border-indigo-500 outline-none text-slate-200 placeholder-slate-500"
      />
      <button
        onClick={save}
        className="px-4 py-2 bg-indigo-600 hover:bg-indigo-500 text-xs font-medium text-white rounded-xl transition-colors shadow-sm"
      >
        Post
      </button>
    </div>
  );
}
