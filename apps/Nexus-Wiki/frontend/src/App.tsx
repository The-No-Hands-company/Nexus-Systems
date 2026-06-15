import { useState, useEffect, useCallback, useMemo } from "react";

// ── Types ──
interface Page { slug: string; title: string; content: string; category?: string | null; tags?: string[]; version: number; created_at: string; updated_at: string; }
interface PageSummary { slug: string; title: string; updated_at: string; }
interface Revision { slug: string; version: number; title: string; content: string; editor_id: string; change_summary: string; created_at: string; }
interface Category { name: string; count: number; }
interface Backlink { slug: string; title: string; }

const API = "/api/v1";
const fj = (u: string, o?: RequestInit) => fetch(u, o).then(r => r.ok ? r.json() : Promise.reject(r.status));

// ── Wiki renderer: [[links]], headings → TOC, infobox, code ──

interface TocItem { id: string; text: string; level: number; }

function parseWiki(text: string): { html: string; toc: TocItem[]; linkedSlugs: string[] } {
  const toc: TocItem[] = [];
  const linkedSlugs: string[] = [];
  let html = text
    .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

  // [[Article Name]] or [[Article Name|display text]]
  html = html.replace(/\[\[([^\]|]+)(?:\|([^\]]+))?\]\]/g, (_, slug, display) => {
    const s = slug.trim().toLowerCase().replace(/\s+/g, "-");
    linkedSlugs.push(s);
    return `<a href="#" onclick="event.preventDefault();window.__nav__('${s}')" class="text-blue-400 hover:underline wiki-link">${display || slug}</a>`;
  });

  // Headings → TOC entries + anchor IDs
  html = html.replace(/^(#{1,3})\s+(.+)$/gm, (_, hashes, text) => {
    const level = hashes.length;
    const id = text.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
    toc.push({ id, text, level });
    const tag = `h${level + 2}`; // h2→h2, h1→h3 etc
    return `<${tag} id="${id}" class="font-bold mt-6 mb-2 ${level === 1 ? "text-2xl border-b border-zinc-800 pb-2" : "text-xl"}">${text}</${tag}>`;
  });

  // Bold, italic, code, lists
  html = html.replace(/\*\*(.+?)\*\*/g, "<strong>$1</strong>");
  html = html.replace(/(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)/g, "<em>$1</em>");
  html = html.replace(/`([^`]+)`/g, "<code class='bg-zinc-800 px-1 rounded text-sm'>$1</code>");
  html = html.replace(/^(\*|-)\s+(.+)$/gm, "<li class='ml-4 list-disc'>$2</li>");
  html = html.replace(/^>\s+(.+)$/gm, "<blockquote class='border-l-2 border-zinc-600 pl-3 italic text-zinc-400'>$1</blockquote>");
  html = html.replace(/\n\n/g, "</p><p class='mb-3'>");
  html = "<p class='mb-3'>" + html + "</p>";

  return { html, toc, linkedSlugs };
}

// ── Simple line diff ──
function lineDiff(oldLines: string[], newLines: string[]): string {
  let result = "";
  const max = Math.max(oldLines.length, newLines.length);
  for (let i = 0; i < max; i++) {
    if (i >= oldLines.length) { result += `<div class="bg-green-900/30 text-green-300">+ ${newLines[i]}</div>`; }
    else if (i >= newLines.length) { result += `<div class="bg-red-900/30 text-red-300">- ${oldLines[i]}</div>`; }
    else if (oldLines[i] !== newLines[i]) {
      result += `<div class="bg-red-900/20 text-red-300">- ${oldLines[i]}</div>`;
      result += `<div class="bg-green-900/20 text-green-300">+ ${newLines[i]}</div>`;
    } else { result += `<div class="text-zinc-400">  ${oldLines[i]}</div>`; }
  }
  return result;
}

// ── App ──
export default function App() {
  const [view, setView] = useState<"browse" | "read" | "edit" | "history" | "diff" | "talk" | "links">("browse");
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
  const [authUser, setAuthUser] = useState(""); const [authPass, setAuthPass] = useState("");

  const loadPages = useCallback(async () => { try { setPages((await fj(`${API}/pages`)).items); } catch {} }, []);
  const loadCategories = useCallback(async () => { try { setCategories((await fj(`${API}/categories`)).categories); } catch {} }, []);
  useEffect(() => { loadPages(); loadCategories(); }, []);

  // Make [[link]] clicks work
  useEffect(() => { (window as any).__nav__ = (s: string) => openPage(s); }, []);

  const openPage = async (s: string) => {
    try { setPage(await fj(`${API}/pages/${s}`)); setSlug(s); setView("read"); } catch {}
  };
  const openEdit = (s: string) => { setSlug(s); setView("edit"); };
  const randomPage = async () => { try { const p = await fj(`${API}/pages/random`); openPage(p.slug); } catch {} };

  const openHistory = async (s: string) => {
    try { setRevisions((await fj(`${API}/pages/${s}/revisions`)).items); setSlug(s); setView("history"); } catch {}
  };
  const openDiff = async (s: string, v1: number, v2: number) => {
    try { setDiffData(await fj(`${API}/diff/${s}?v1=${v1}&v2=${v2}`)); setSlug(s); setView("diff"); } catch {}
  };
  const openTalk = async (s: string) => {
    try { setTalkPage(await fj(`${API}/pages/${s}/talk`)); setSlug(s); setView("talk"); } catch {}
  };
  const openLinks = async (s: string) => {
    try { setBacklinks((await fj(`${API}/pages/${s}/links`)).backlinks); setSlug(s); setView("links"); } catch {}
  };
  const goBrowse = () => { setView("browse"); setSearch(""); setSelectedCat(""); };

  const login = async () => {
    try { const u = await fj(`${API}/auth/login`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ username: authUser, password: authPass }) }); setUser(u); } catch {}
  };
  const register = async () => {
    try { const u = await fj(`${API}/auth/register`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ username: authUser, password: authPass }) }); setUser(u); } catch {}
  };

  const filtered = pages.filter(p => {
    if (search && !p.title.toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-200">
      <header className="border-b border-zinc-800 bg-zinc-900 sticky top-0 z-10">
        <div className="max-w-5xl mx-auto px-4 py-3 flex items-center gap-4">
          <button onClick={goBrowse} className="text-xl font-bold text-blue-400 hover:text-blue-300 shrink-0">Nexus-Wiki</button>
          <div className="flex-1 flex gap-2">
            <input value={search} onChange={e => setSearch(e.target.value)} onKeyDown={e => e.key === "Enter" && goBrowse()}
              placeholder="Search..." className="flex-1 bg-zinc-800 px-3 py-1.5 rounded text-sm border border-zinc-700 focus:border-blue-500 outline-none" />
            <button onClick={randomPage} className="px-3 py-1.5 text-sm bg-zinc-800 rounded hover:bg-zinc-700 shrink-0">Random</button>
            <button onClick={() => openEdit("")} className="px-3 py-1.5 text-sm bg-blue-600 rounded hover:bg-blue-500 shrink-0">+ New</button>
          </div>
          {user ? (
            <span className="text-xs text-zinc-500 shrink-0">{user.username}</span>
          ) : (
            <div className="flex gap-1 shrink-0">
              <input value={authUser} onChange={e => setAuthUser(e.target.value)} placeholder="user" className="w-16 bg-zinc-800 px-1.5 py-1 rounded text-xs border border-zinc-700" />
              <input value={authPass} onChange={e => setAuthPass(e.target.value)} type="password" placeholder="pass" className="w-16 bg-zinc-800 px-1.5 py-1 rounded text-xs border border-zinc-700" />
              <button onClick={login} className="px-2 py-1 text-xs bg-zinc-700 rounded hover:bg-zinc-600">Login</button>
              <button onClick={register} className="px-2 py-1 text-xs bg-zinc-800 rounded hover:bg-zinc-700">Reg</button>
            </div>
          )}
        </div>
      </header>

      <main className="max-w-5xl mx-auto px-4 py-6 flex gap-6">
        {/* Sidebar */}
        {view === "browse" && (
          <aside className="w-48 shrink-0">
            <h3 className="text-xs uppercase text-zinc-500 font-semibold mb-2">Categories</h3>
            <div className="space-y-1">
              <button onClick={() => setSelectedCat("")} className={`block w-full text-left text-sm px-2 py-0.5 rounded ${!selectedCat ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800"}`}>All Pages</button>
              {categories.map(c => (
                <button key={c.name} onClick={() => setSelectedCat(c.name)} className={`block w-full text-left text-sm px-2 py-0.5 rounded flex justify-between ${selectedCat === c.name ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800"}`}>
                  <span>{c.name}</span><span className="text-zinc-600">{c.count}</span>
                </button>
              ))}
            </div>
          </aside>
        )}

        <div className="flex-1 min-w-0">
          {/* ── BROWSE ── */}
          {view === "browse" && (
            <div>
              <h2 className="text-lg font-semibold mb-4">{search ? `Search: "${search}"` : "All Articles"}</h2>
              <div className="space-y-1">
                {filtered.map(p => (
                  <button key={p.slug} onClick={() => openPage(p.slug)} className="block w-full text-left px-3 py-2 rounded hover:bg-zinc-800/50">
                    <span className="text-blue-400">{p.title}</span>
                    <span className="text-xs text-zinc-600 ml-3">{new Date(p.updated_at).toLocaleDateString()}</span>
                  </button>
                ))}
              </div>
            </div>
          )}

          {/* ── READ ── */}
          {view === "read" && page && (
            <div>
              <Toolbar slug={slug} onEdit={openEdit} onHistory={openHistory} onTalk={openTalk} onLinks={openLinks} onBrowse={goBrowse} />
              <h1 className="text-3xl font-bold mt-2 mb-1">{page.title}</h1>
              {page.category && <span className="text-xs bg-zinc-800 px-2 py-0.5 rounded mr-2">{page.category}</span>}
              {page.tags?.map(t => <span key={t} className="text-xs text-zinc-500 mr-2">#{t}</span>)}
              <p className="text-xs text-zinc-600 mt-2 mb-6">Version {page.version} · Updated {new Date(page.updated_at).toLocaleString()}</p>

              {/* Table of Contents */}
              {(() => { const { html, toc } = parseWiki(page.content); return (
                <>
                  {toc.length > 2 && (
                    <div className="bg-zinc-900 border border-zinc-800 rounded p-3 mb-6 text-sm">
                      <h4 className="font-semibold mb-2">Contents</h4>
                      <ul className="space-y-1">
                        {toc.map(t => (
                          <li key={t.id} style={{ paddingLeft: `${(t.level-1) * 12}px` }}>
                            <a href={`#${t.id}`} className="text-blue-400 hover:underline">{t.text}</a>
                          </li>
                        ))}
                      </ul>
                    </div>
                  )}
                  <div className="prose max-w-none leading-relaxed" dangerouslySetInnerHTML={{ __html }} />
                </>
              ); })()}
            </div>
          )}

          {/* ── EDIT ── */}
          {view === "edit" && <Editor slug={slug} page={page} user={user} onSave={s => openPage(s)} onCancel={() => slug ? openPage(slug) : goBrowse()} />}

          {/* ── HISTORY ── */}
          {view === "history" && (
            <div>
              <button onClick={() => slug && openPage(slug)} className="text-sm text-zinc-500 hover:text-zinc-300 mb-4 block">← Back</button>
              <h2 className="text-lg font-semibold mb-4">Revision History — {page?.title}</h2>
              <div className="space-y-2">
                {revisions.map((r, i) => (
                  <div key={r.version} className="p-3 border border-zinc-800 rounded flex justify-between items-center">
                    <div>
                      <span className="font-semibold text-sm">v{r.version}</span>
                      <span className="text-xs text-zinc-500 ml-2">{new Date(r.created_at).toLocaleString()}</span>
                      <span className="text-xs text-zinc-600 ml-2">— {r.editor_id}: {r.change_summary}</span>
                    </div>
                    {i < revisions.length - 1 && (
                      <button onClick={() => openDiff(slug, r.version, revisions[i+1].version)}
                        className="text-xs px-2 py-0.5 bg-zinc-800 rounded hover:bg-zinc-700">Diff</button>
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* ── DIFF ── */}
          {view === "diff" && diffData && (
            <div>
              <button onClick={() => slug && openHistory(slug)} className="text-sm text-zinc-500 hover:text-zinc-300 mb-4 block">← Back</button>
              <h2 className="text-lg font-semibold mb-2">Changes: v{diffData.v2} → v{diffData.v1}</h2>
              <div className="bg-zinc-900 border border-zinc-800 rounded p-4 font-mono text-xs overflow-x-auto" style={{ maxHeight: "70vh" }}>
                <div dangerouslySetInnerHTML={{ __html: lineDiff(
                  (revisions.find(r => r.version === diffData.v2)?.content || "").split("\n"),
                  (revisions.find(r => r.version === diffData.v1)?.content || "").split("\n")
                )}} />
              </div>
            </div>
          )}

          {/* ── TALK ── */}
          {view === "talk" && (
            <div>
              <button onClick={() => slug && openPage(slug)} className="text-sm text-zinc-500 hover:text-zinc-300 mb-4 block">← Back to article</button>
              <h2 className="text-lg font-semibold mb-4">Talk: {page?.title}</h2>
              {talkPage && (
                <div className="prose max-w-none" dangerouslySetInnerHTML={{ __html: parseWiki(talkPage.content).html }} />
              )}
              <div className="mt-6 p-4 bg-zinc-900 border border-zinc-800 rounded">
                <h3 className="text-sm font-semibold mb-2">New discussion</h3>
                <TalkEditor slug={slug} user={user} onSaved={() => openTalk(slug)} />
              </div>
            </div>
          )}

          {/* ── WHAT LINKS HERE ── */}
          {view === "links" && (
            <div>
              <button onClick={() => slug && openPage(slug)} className="text-sm text-zinc-500 hover:text-zinc-300 mb-4 block">← Back to article</button>
              <h2 className="text-lg font-semibold mb-4">What links here — {page?.title}</h2>
              {backlinks.length === 0 && <p className="text-zinc-500 text-sm">No pages link here yet.</p>}
              <ul className="space-y-1">
                {backlinks.map(b => (
                  <li key={b.slug}><button onClick={() => openPage(b.slug)} className="text-blue-400 hover:underline text-sm">{b.title}</button></li>
                ))}
              </ul>
            </div>
          )}
        </div>
      </main>
      <footer className="border-t border-zinc-800 py-4 px-4 text-center text-xs text-zinc-600">
        Nexus-Wiki — A free, open-source knowledge base. Powered by the Nexus Systems ecosystem.
      </footer>
    </div>
  );
}

// ── Toolbar ──
function Toolbar({ slug, onEdit, onHistory, onTalk, onLinks, onBrowse }: any) {
  return (
    <div className="flex items-center gap-2 text-sm mb-2 flex-wrap">
      <button onClick={onBrowse} className="text-zinc-500 hover:text-zinc-300">← Back</button>
      <span className="text-zinc-700">|</span>
      <button onClick={() => onEdit(slug)} className="hover:text-blue-300">Edit</button>
      <button onClick={() => onHistory(slug)} className="hover:text-blue-300">History</button>
      <button onClick={() => onTalk(slug)} className="hover:text-blue-300">Talk</button>
      <button onClick={() => onLinks(slug)} className="hover:text-blue-300">What links here</button>
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
    if (!title.trim() || !content.trim()) return; setSaving(true);
    try {
      const method = slug && page ? "PATCH" : "POST";
      const url = slug && page ? `${API}/pages/${slug}` : `${API}/pages`;
      const body: any = { title: title.trim(), content: content.trim(), editor_id: user?.username || "anonymous", change_summary: "Edited" };
      if (category.trim()) body.category = category.trim();
      const res = await fetch(url, { method, headers: { "content-type": "application/json" }, body: JSON.stringify(body) });
      if (res.ok) { const p = await res.json(); onSave(p.slug); }
    } finally { setSaving(false); }
  };

  return (
    <div>
      <h2 className="text-lg font-semibold mb-4">{slug && page ? "Edit" : "Create"} Article</h2>
      <input value={title} onChange={e => setTitle(e.target.value)} placeholder="Title" className="w-full bg-zinc-800 px-3 py-2 rounded mb-2 border border-zinc-700 focus:border-blue-500 outline-none text-lg font-semibold" />
      <input value={category} onChange={e => setCategory(e.target.value)} placeholder="Category (optional)" className="w-full bg-zinc-800 px-3 py-2 rounded mb-2 border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
      <textarea value={content} onChange={e => setContent(e.target.value)} placeholder="Write in wiki markup...&#10;&#10;= Heading&#10;[[Linked Page]]&#10;[[Page|display text]]&#10;* list item&#10;> blockquote&#10;`code`" rows={22}
        className="w-full bg-zinc-800 px-3 py-2 rounded mb-3 border border-zinc-700 focus:border-blue-500 outline-none text-sm font-mono leading-relaxed" />
      <div className="flex gap-2">
        <button onClick={save} disabled={saving} className="px-4 py-2 bg-blue-600 rounded hover:bg-blue-500 disabled:opacity-50 text-sm">{saving ? "Saving..." : "Save"}</button>
        <button onClick={onCancel} className="px-4 py-2 bg-zinc-800 rounded hover:bg-zinc-700 text-sm">Cancel</button>
      </div>
      <p className="text-xs text-zinc-600 mt-2">Wiki syntax: [[links]], = Heading, **bold**, *italic*, `code`, * list, &gt; blockquote</p>
    </div>
  );
}

// ── Talk Editor ──
function TalkEditor({ slug, user, onSaved }: { slug: string; user: any; onSaved: () => void }) {
  const [content, setContent] = useState("");
  const save = async () => {
    if (!content.trim()) return;
    await fetch(`${API}/pages/${slug}/talk`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: `Talk: ${slug}`, content: content.trim(), editor_id: user?.username || "anonymous" })
    });
    setContent(""); onSaved();
  };
  return (
    <div className="flex gap-2">
      <input value={content} onChange={e => setContent(e.target.value)} placeholder="Add a comment..."
        className="flex-1 bg-zinc-800 px-3 py-1.5 rounded text-sm border border-zinc-700 focus:border-blue-500 outline-none" />
      <button onClick={save} className="px-3 py-1.5 bg-blue-600 rounded text-sm hover:bg-blue-500">Post</button>
    </div>
  );
}
