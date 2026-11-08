import React, { useState, useEffect, useCallback, useMemo, useRef } from "react";
import {
  BookOpen,
  Pin,
  PinOff,
  Plus,
  Search,
  Trash2,
  Tag,
  Folder,
  Clock,
  Check,
  FileText,
  Eye,
  Edit3,
  Columns,
  ShieldCheck,
  Share2,
  Download,
  Network,
  Link as LinkIcon,
  ExternalLink,
  Printer,
} from "lucide-react";

interface Note {
  id: string;
  title: string;
  content: string;
  notebook: string;
  tags: string;
  pinned: number;
  createdAt: string;
  updatedAt: string;
}

interface Notebook {
  id: string;
  name: string;
  description: string;
  noteCount: number;
  createdAt: string;
}

// Fast markdown parser with [[WikiLinks]] support
function renderMarkdown(md: string): string {
  if (!md) return '<p class="text-zinc-600 italic">No content yet. Start typing...</p>';
  let html = md
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");

  // Code blocks
  html = html.replace(/```([a-z0-9_-]*)\n([\s\S]*?)```/g, (_, lang, code) => {
    return `<pre class="bg-zinc-900 border border-zinc-800 p-3 rounded-lg overflow-x-auto text-sm my-3 font-mono text-emerald-400"><code>${code}</code></pre>`;
  });

  // Inline code
  html = html.replace(/`([^`]+)`/g, '<code class="bg-zinc-800 text-emerald-300 px-1.5 py-0.5 rounded text-xs font-mono">$1</code>');

  // Headings
  html = html.replace(/^### (.*$)/gim, '<h3 class="text-lg font-semibold text-zinc-100 mt-4 mb-2">$1</h3>');
  html = html.replace(/^## (.*$)/gim, '<h2 class="text-xl font-bold text-zinc-100 mt-5 mb-2 pb-1 border-b border-zinc-800">$1</h2>');
  html = html.replace(/^# (.*$)/gim, '<h1 class="text-2xl font-black text-white mt-6 mb-3 pb-2 border-b border-zinc-800">$1</h1>');

  // Blockquotes
  html = html.replace(/^> (.*$)/gim, '<blockquote class="border-l-4 border-emerald-500/60 pl-3 italic text-zinc-400 my-2">$1</blockquote>');

  // [[WikiLinks]]
  html = html.replace(/\[\[([^\]]+)\]\]/g, (_, linkText) => {
    return `<span class="wikilink inline-flex items-center gap-1 text-emerald-400 bg-emerald-950/60 hover:bg-emerald-900/80 border border-emerald-700/60 px-1.5 py-0.5 rounded text-xs cursor-pointer select-none transition-colors" data-wikilink="${linkText}">🔗 <strong>${linkText}</strong></span>`;
  });

  // Tables
  html = html.replace(/(?:^|\n)(\|.+?\|\n\|[-:| ]+\|\n(?:\|.+?\|\n?)+)/g, (tableMatch) => {
    const rows = tableMatch.trim().split("\n");
    if (rows.length < 3) return tableMatch;
    const headerCols = rows[0].split("|").slice(1, -1).map(c => `<th class="border border-zinc-700 bg-zinc-800/80 px-3 py-1.5 text-left font-semibold text-zinc-200">${c.trim()}</th>`).join("");
    const bodyRows = rows.slice(2).map(r => {
      const cols = r.split("|").slice(1, -1).map(c => `<td class="border border-zinc-800 px-3 py-1.5 text-zinc-300">${c.trim()}</td>`).join("");
      return `<tr class="hover:bg-zinc-900/60">${cols}</tr>`;
    }).join("");
    return `<div class="overflow-x-auto my-3"><table class="w-full text-xs border-collapse border border-zinc-700"><thead><tr>${headerCols}</tr></thead><tbody>${bodyRows}</tbody></table></div>`;
  });

  // Checkboxes
  html = html.replace(/^- \[ \] (.*$)/gim, '<div class="flex items-center gap-2 my-1"><input type="checkbox" disabled class="accent-emerald-500 rounded"><span class="text-zinc-300">$1</span></div>');
  html = html.replace(/^- \[x\] (.*$)/gim, '<div class="flex items-center gap-2 my-1"><input type="checkbox" checked disabled class="accent-emerald-500 rounded"><span class="line-through text-zinc-500">$1</span></div>');

  // Lists
  html = html.replace(/^\* (.*$)/gim, '<li class="ml-4 list-disc text-zinc-300">$1</li>');
  html = html.replace(/^- (.*$)/gim, '<li class="ml-4 list-disc text-zinc-300">$1</li>');

  // Bold / Italic
  html = html.replace(/\*\*([^*]+)\*\*/g, '<strong class="font-bold text-zinc-100">$1</strong>');
  html = html.replace(/\*([^*]+)\*/g, '<em class="italic text-zinc-300">$1</em>');

  // Paragraph breaks
  html = html.replace(/\n\n+/g, '</p><p class="my-2.5 leading-relaxed text-zinc-300">');
  return `<p class="my-2.5 leading-relaxed text-zinc-300">${html}</p>`;
}

// Interactive 2D Graph Component
function GraphView({
  notes,
  activeNoteId,
  onSelectNote,
}: {
  notes: Note[];
  activeNoteId: string | null;
  onSelectNote: (note: Note) => void;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    let animFrame: number;
    const width = (canvas.width = canvas.parentElement?.clientWidth || 800);
    const height = (canvas.height = canvas.parentElement?.clientHeight || 600);

    // Build graph nodes
    const nodeMap = new Map<string, { x: number; y: number; vx: number; vy: number; note: Note }>();
    const angleStep = (Math.PI * 2) / Math.max(notes.length, 1);
    const radius = Math.min(width, height) * 0.35;

    notes.forEach((note, i) => {
      nodeMap.set(note.id, {
        x: width / 2 + Math.cos(i * angleStep) * radius + (Math.random() - 0.5) * 40,
        y: height / 2 + Math.sin(i * angleStep) * radius + (Math.random() - 0.5) * 40,
        vx: 0,
        vy: 0,
        note,
      });
    });

    // Build edges from [[WikiLinks]]
    const edges: Array<{ from: string; to: string }> = [];
    notes.forEach(fromNote => {
      const matches = fromNote.content.matchAll(/\[\[(.*?)\]\]/g);
      for (const m of matches) {
        const targetTitle = m[1].trim().toLowerCase();
        const toNote = notes.find(n => n.title.trim().toLowerCase() === targetTitle);
        if (toNote && toNote.id !== fromNote.id) {
          edges.push({ from: fromNote.id, to: toNote.id });
        }
      }
    });

    let iteration = 0;
    const render = () => {
      ctx.fillStyle = "#09090b";
      ctx.fillRect(0, 0, width, height);

      // Simple relaxation simulation
      if (iteration < 120) {
        iteration++;
        // Repulsion between all pairs
        const nodes = Array.from(nodeMap.values());
        for (let i = 0; i < nodes.length; i++) {
          for (let j = i + 1; j < nodes.length; j++) {
            const dx = nodes[j].x - nodes[i].x;
            const dy = nodes[j].y - nodes[i].y;
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;
            if (dist < 180) {
              const force = (180 - dist) / 180 * 0.8;
              nodes[i].x -= (dx / dist) * force;
              nodes[i].y -= (dy / dist) * force;
              nodes[j].x += (dx / dist) * force;
              nodes[j].y += (dy / dist) * force;
            }
          }
        }
        // Spring attraction for edges
        edges.forEach(({ from, to }) => {
          const a = nodeMap.get(from);
          const b = nodeMap.get(to);
          if (a && b) {
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;
            const force = (dist - 100) * 0.03;
            a.x += (dx / dist) * force;
            a.y += (dy / dist) * force;
            b.x -= (dx / dist) * force;
            b.y -= (dy / dist) * force;
          }
        });
      }

      // Draw edges
      edges.forEach(({ from, to }) => {
        const a = nodeMap.get(from);
        const b = nodeMap.get(to);
        if (a && b) {
          ctx.beginPath();
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
          ctx.strokeStyle = "#059669";
          ctx.lineWidth = 1.5;
          ctx.stroke();
        }
      });

      // Draw nodes
      nodeMap.forEach(({ x, y, note }) => {
        const isActive = note.id === activeNoteId;
        ctx.beginPath();
        ctx.arc(x, y, isActive ? 9 : 6, 0, Math.PI * 2);
        ctx.fillStyle = isActive ? "#10b981" : "#34d399";
        ctx.shadowColor = "#10b981";
        ctx.shadowBlur = isActive ? 12 : 4;
        ctx.fill();
        ctx.shadowBlur = 0;

        // Label
        ctx.fillStyle = isActive ? "#ffffff" : "#a1a1aa";
        ctx.font = isActive ? "bold 11px sans-serif" : "10px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText(note.title || "Untitled", x, y + 18);
      });

      animFrame = requestAnimationFrame(render);
    };

    render();

    // Click handler on canvas
    const handleClick = (e: MouseEvent) => {
      const rect = canvas.getBoundingClientRect();
      const clickX = e.clientX - rect.left;
      const clickY = e.clientY - rect.top;

      nodeMap.forEach(({ x, y, note }) => {
        const dist = Math.hypot(clickX - x, clickY - y);
        if (dist <= 15) {
          onSelectNote(note);
        }
      });
    };

    canvas.addEventListener("click", handleClick);

    return () => {
      cancelAnimationFrame(animFrame);
      canvas.removeEventListener("click", handleClick);
    };
  }, [notes, activeNoteId, onSelectNote]);

  return (
    <div className="relative w-full h-full bg-zinc-950 flex flex-col items-center justify-center">
      <div className="absolute top-4 left-4 z-10 px-3 py-1.5 rounded-lg bg-zinc-900/80 border border-zinc-800 text-xs text-zinc-400">
        Interactive Knowledge Graph • {notes.length} notes connected
      </div>
      <canvas ref={canvasRef} className="w-full h-full cursor-pointer" />
    </div>
  );
}

export default function App() {
  const [notebooks, setNotebooks] = useState<Notebook[]>([]);
  const [notes, setNotes] = useState<Note[]>([]);
  const [selectedNotebook, setSelectedNotebook] = useState<string | null>(null);
  const [filterPinned, setFilterPinned] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [selectedNoteId, setSelectedNoteId] = useState<string | null>(null);

  // Active Note Editor State
  const [editTitle, setEditTitle] = useState("");
  const [editContent, setEditContent] = useState("");
  const [editNotebook, setEditNotebook] = useState("General");
  const [editTags, setEditTags] = useState("");
  const [saveStatus, setSaveStatus] = useState<"saved" | "saving" | "unsaved">("saved");
  const [editorMode, setEditorMode] = useState<"edit" | "split" | "preview" | "graph">("split");
  const [showExportMenu, setShowExportMenu] = useState(false);

  // [[WikiLink]] autocomplete state
  const [wikiSuggestOpen, setWikiSuggestOpen] = useState(false);
  const [wikiSuggestQuery, setWikiSuggestQuery] = useState("");
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // New Notebook Input Modal/State
  const [isAddingNotebook, setIsAddingNotebook] = useState(false);
  const [newNotebookName, setNewNotebookName] = useState("");

  const activeNote = useMemo(() => {
    return notes.find((n) => n.id === selectedNoteId) || null;
  }, [notes, selectedNoteId]);

  // Backlinks for active note
  const backlinks = useMemo(() => {
    if (!activeNote || !activeNote.title.trim()) return [];
    const titleLower = activeNote.title.trim().toLowerCase();
    return notes.filter(
      (n) => n.id !== activeNote.id && n.content.toLowerCase().includes(`[[${titleLower}]]`)
    );
  }, [activeNote, notes]);

  // Load notebooks & notes on mount
  const refreshNotebooks = useCallback(async () => {
    try {
      const res = await fetch("/api/v1/notes/notebooks");
      if (res.ok) {
        const data = await res.json();
        setNotebooks(data);
      }
    } catch (err) {
      console.error("Failed to load notebooks", err);
    }
  }, []);

  const selectedNoteIdRef = useRef<string | null>(selectedNoteId);
  selectedNoteIdRef.current = selectedNoteId;

  const refreshNotes = useCallback(async () => {
    try {
      let url = "/api/v1/notes";
      if (searchQuery.trim()) {
        url = `/api/v1/notes/search?q=${encodeURIComponent(searchQuery.trim())}`;
      } else if (selectedNotebook) {
        url = `/api/v1/notes?notebook=${encodeURIComponent(selectedNotebook)}`;
      }
      const res = await fetch(url);
      if (res.ok) {
        const data: Note[] = await res.json();
        setNotes(data);
        if (data.length > 0 && !selectedNoteIdRef.current) {
          selectNote(data[0]);
        }
      }
    } catch (err) {
      console.error("Failed to load notes", err);
    }
  }, [selectedNotebook, searchQuery]);

  useEffect(() => {
    refreshNotebooks();
  }, [refreshNotebooks]);

  useEffect(() => {
    refreshNotes();
  }, [refreshNotes]);

  const selectNote = (note: Note) => {
    setSelectedNoteId(note.id);
    setEditTitle(note.title);
    setEditContent(note.content);
    setEditNotebook(note.notebook);
    setEditTags(note.tags);
    setSaveStatus("saved");
  };

  const handleCreateNote = async (title = "Untitled Note", initialContent = "") => {
    try {
      const res = await fetch("/api/v1/notes", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          title,
          content: initialContent || `# ${title}\n\nWrite your thoughts here...`,
          notebook: selectedNotebook || "General",
          tags: "",
        }),
      });
      if (res.ok) {
        const newNote: Note = await res.json();
        setNotes((prev) => [newNote, ...prev]);
        selectNote(newNote);
        refreshNotebooks();
      }
    } catch (err) {
      console.error("Failed to create note", err);
    }
  };

  const handleCreateNotebook = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newNotebookName.trim()) return;
    try {
      const res = await fetch("/api/v1/notes/notebooks", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: newNotebookName.trim() }),
      });
      if (res.ok) {
        setNewNotebookName("");
        setIsAddingNotebook(false);
        refreshNotebooks();
      }
    } catch (err) {
      console.error("Failed to create notebook", err);
    }
  };

  const saveTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const saveCurrentNote = useCallback(
    async (title: string, content: string, notebook: string, tags: string) => {
      if (!selectedNoteId) return;
      setSaveStatus("saving");
      try {
        const res = await fetch(`/api/v1/notes/${selectedNoteId}`, {
          method: "PATCH",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ title, content, notebook, tags }),
        });
        if (res.ok) {
          const updated: Note = await res.json();
          setNotes((prev) => prev.map((n) => (n.id === updated.id ? updated : n)));
          setSaveStatus("saved");
          refreshNotebooks();
        } else {
          setSaveStatus("unsaved");
        }
      } catch {
        setSaveStatus("unsaved");
      }
    },
    [selectedNoteId, refreshNotebooks]
  );

  const onEditChange = (updates: { title?: string; content?: string; notebook?: string; tags?: string }) => {
    const nextTitle = updates.title !== undefined ? updates.title : editTitle;
    const nextContent = updates.content !== undefined ? updates.content : editContent;
    const nextNotebook = updates.notebook !== undefined ? updates.notebook : editNotebook;
    const nextTags = updates.tags !== undefined ? updates.tags : editTags;

    if (updates.title !== undefined) setEditTitle(nextTitle);
    if (updates.content !== undefined) {
      setEditContent(nextContent);

      // Check for [[ wikilink typing
      const cursor = textareaRef.current?.selectionStart ?? nextContent.length;
      const textBeforeCursor = nextContent.slice(0, cursor);
      const match = textBeforeCursor.match(/\[\[([^\]\n]*)$/);
      if (match) {
        setWikiSuggestOpen(true);
        setWikiSuggestQuery(match[1]);
      } else {
        setWikiSuggestOpen(false);
      }
    }
    if (updates.notebook !== undefined) setEditNotebook(nextNotebook);
    if (updates.tags !== undefined) setEditTags(nextTags);

    setSaveStatus("unsaved");

    if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    saveTimerRef.current = setTimeout(() => {
      saveCurrentNote(nextTitle, nextContent, nextNotebook, nextTags);
    }, 800);
  };

  const insertWikilink = (targetTitle: string) => {
    if (!textareaRef.current) return;
    const textarea = textareaRef.current;
    const cursor = textarea.selectionStart;
    const textBefore = editContent.slice(0, cursor);
    const textAfter = editContent.slice(cursor);
    const linkStart = textBefore.lastIndexOf("[[");
    if (linkStart !== -1) {
      const newText = textBefore.slice(0, linkStart) + `[[${targetTitle}]]` + textAfter;
      onEditChange({ content: newText });
    }
    setWikiSuggestOpen(false);
  };

  const handleTogglePin = async (e: React.MouseEvent, noteId: string) => {
    e.stopPropagation();
    try {
      const res = await fetch(`/api/v1/notes/${noteId}/pin`, { method: "POST" });
      if (res.ok) {
        const updated: Note = await res.json();
        setNotes((prev) => prev.map((n) => (n.id === updated.id ? updated : n)));
      }
    } catch (err) {
      console.error("Failed to toggle pin", err);
    }
  };

  const handleDeleteNote = async (e: React.MouseEvent, noteId: string) => {
    e.stopPropagation();
    if (!confirm("Are you sure you want to delete this note?")) return;
    try {
      const res = await fetch(`/api/v1/notes/${noteId}`, { method: "DELETE" });
      if (res.ok) {
        setNotes((prev) => prev.filter((n) => n.id !== noteId));
        if (selectedNoteId === noteId) {
          const remaining = notes.filter((n) => n.id !== noteId);
          if (remaining.length > 0) selectNote(remaining[0]);
          else setSelectedNoteId(null);
        }
        refreshNotebooks();
      }
    } catch (err) {
      console.error("Failed to delete note", err);
    }
  };

  // Click on rendered wikilink in preview
  const handlePreviewClick = (e: React.MouseEvent<HTMLDivElement>) => {
    const target = e.target as HTMLElement;
    const wikilinkEl = target.closest("[data-wikilink]");
    if (wikilinkEl) {
      const linkTitle = wikilinkEl.getAttribute("data-wikilink");
      if (linkTitle) {
        const found = notes.find((n) => n.title.trim().toLowerCase() === linkTitle.trim().toLowerCase());
        if (found) {
          selectNote(found);
        } else {
          if (confirm(`Note "${linkTitle}" does not exist yet. Create it now?`)) {
            handleCreateNote(linkTitle);
          }
        }
      }
    }
  };

  // Export handlers
  const exportAsMarkdown = () => {
    if (!activeNote) return;
    const blob = new Blob([editContent], { type: "text/markdown;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${editTitle.replace(/[^a-zA-Z0-9_-]/g, "_") || "note"}.md`;
    a.click();
    URL.revokeObjectURL(url);
    setShowExportMenu(false);
  };

  const exportAsHtml = () => {
    if (!activeNote) return;
    const fullHtml = `<!DOCTYPE html><html><head><meta charset="utf-8"><title>${editTitle}</title><style>body{font-family:sans-serif;max-width:800px;margin:40px auto;padding:0 20px;line-height:1.6;color:#27272a;background:#fff;}pre{background:#f4f4f5;padding:12px;border-radius:6px;}code{font-family:monospace;}</style></head><body><h1>${editTitle}</h1>${renderMarkdown(editContent)}</body></html>`;
    const blob = new Blob([fullHtml], { type: "text/html;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${editTitle.replace(/[^a-zA-Z0-9_-]/g, "_") || "note"}.html`;
    a.click();
    URL.revokeObjectURL(url);
    setShowExportMenu(false);
  };

  const displayedNotes = useMemo(() => {
    let result = notes;
    if (filterPinned) {
      result = result.filter((n) => n.pinned === 1);
    }
    return result;
  }, [notes, filterPinned]);

  const wordCount = useMemo(() => {
    return editContent.trim() ? editContent.trim().split(/\s+/).length : 0;
  }, [editContent]);

  return (
    <div className="flex h-screen w-screen bg-zinc-950 font-sans text-zinc-100 antialiased overflow-hidden select-none">
      {/* ── Left Column: Notebooks & Filters ── */}
      <aside className="w-64 flex-shrink-0 bg-zinc-900 border-r border-zinc-800 flex flex-col justify-between">
        <div className="p-4 flex flex-col gap-4">
          <div className="flex items-center gap-2.5 px-2">
            <div className="w-8 h-8 rounded-lg bg-emerald-500/20 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
              <FileText className="w-4 h-4" />
            </div>
            <div>
              <h1 className="text-sm font-semibold tracking-wide text-zinc-100">Nexus Notes</h1>
              <p className="text-[11px] text-zinc-500">Bi-directional Knowledge</p>
            </div>
          </div>

          <div className="flex flex-col gap-1 text-xs">
            <button
              onClick={() => {
                setSelectedNotebook(null);
                setFilterPinned(false);
              }}
              className={`flex items-center justify-between px-3 py-2 rounded-lg transition-colors ${
                !selectedNotebook && !filterPinned
                  ? "bg-emerald-500/15 text-emerald-300 font-medium border border-emerald-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <BookOpen className="w-3.5 h-3.5" />
                <span>All Notes</span>
              </div>
              <span className="text-[10px] bg-zinc-800 px-1.5 py-0.5 rounded text-zinc-400">
                {notes.length}
              </span>
            </button>

            <button
              onClick={() => {
                setSelectedNotebook(null);
                setFilterPinned(true);
              }}
              className={`flex items-center justify-between px-3 py-2 rounded-lg transition-colors ${
                filterPinned
                  ? "bg-amber-500/15 text-amber-300 font-medium border border-amber-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <Pin className="w-3.5 h-3.5" />
                <span>Pinned</span>
              </div>
              <span className="text-[10px] bg-zinc-800 px-1.5 py-0.5 rounded text-zinc-400">
                {notes.filter((n) => n.pinned === 1).length}
              </span>
            </button>
          </div>

          <div className="pt-2">
            <div className="flex items-center justify-between px-2 mb-2">
              <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-500">
                Notebooks
              </span>
              <button
                onClick={() => setIsAddingNotebook(true)}
                className="text-zinc-400 hover:text-emerald-400 transition-colors p-1 rounded hover:bg-zinc-800"
                title="Create Notebook"
              >
                <Plus className="w-3.5 h-3.5" />
              </button>
            </div>

            {isAddingNotebook && (
              <form onSubmit={handleCreateNotebook} className="mb-2 px-1">
                <input
                  type="text"
                  autoFocus
                  placeholder="Notebook name..."
                  value={newNotebookName}
                  onChange={(e) => setNewNotebookName(e.target.value)}
                  onBlur={() => {
                    if (!newNotebookName.trim()) setIsAddingNotebook(false);
                  }}
                  className="w-full bg-zinc-950 border border-emerald-500/50 rounded-lg px-2.5 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:ring-1 focus:ring-emerald-500"
                />
              </form>
            )}

            <div className="flex flex-col gap-0.5 max-h-56 overflow-y-auto">
              {notebooks.map((nb) => (
                <button
                  key={nb.id}
                  onClick={() => {
                    setSelectedNotebook(nb.name);
                    setFilterPinned(false);
                  }}
                  className={`flex items-center justify-between px-3 py-1.5 rounded-lg text-xs transition-colors ${
                    selectedNotebook === nb.name
                      ? "bg-zinc-800 text-zinc-100 font-medium"
                      : "text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800"
                  }`}
                >
                  <div className="flex items-center gap-2 truncate">
                    <Folder className="w-3 h-3 text-zinc-500 flex-shrink-0" />
                    <span className="truncate">{nb.name}</span>
                  </div>
                  {nb.noteCount > 0 && (
                    <span className="text-[10px] text-zinc-500">{nb.noteCount}</span>
                  )}
                </button>
              ))}
            </div>
          </div>
        </div>

        {/* Footer Identity */}
        <div className="p-3 border-t border-zinc-800/80 bg-zinc-950/40 text-[11px] text-zinc-500 flex items-center justify-between">
          <div className="flex items-center gap-1.5">
            <ShieldCheck className="w-3.5 h-3.5 text-emerald-400" />
            <span>Sovereign Storage</span>
          </div>
          <span className="font-mono text-[10px] text-zinc-600">v1.0</span>
        </div>
      </aside>

      {/* ── Middle Column: Note Cards List ── */}
      <section className="w-72 flex-shrink-0 bg-zinc-950 border-r border-zinc-800 flex flex-col">
        {/* Search & New Note header */}
        <div className="p-3 border-b border-zinc-800 flex flex-col gap-2.5">
          <div className="flex items-center justify-between">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
              {selectedNotebook || (filterPinned ? "Pinned Notes" : "All Notes")}
            </h2>
            <button
              onClick={() => handleCreateNote()}
              className="flex items-center gap-1 bg-emerald-500 hover:bg-emerald-400 text-zinc-950 font-medium text-xs px-2.5 py-1 rounded-md transition-colors shadow-sm"
            >
              <Plus className="w-3.5 h-3.5" />
              <span>Note</span>
            </button>
          </div>

          <div className="relative">
            <Search className="w-3.5 h-3.5 text-zinc-500 absolute left-2.5 top-2.5 pointer-events-none" />
            <input
              type="text"
              placeholder="Search notes content..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full bg-zinc-900 border border-zinc-800 rounded-lg pl-8 pr-3 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:border-zinc-700"
            />
          </div>
        </div>

        {/* Notes Scrollable List with live search highlight */}
        <div className="flex-1 overflow-y-auto divide-y divide-zinc-850">
          {displayedNotes.length === 0 ? (
            <div className="p-8 text-center text-xs text-zinc-600">
              No notes found. Create one!
            </div>
          ) : (
            displayedNotes.map((note) => {
              const isSelected = note.id === selectedNoteId;
              const dateStr = new Date(note.updatedAt).toLocaleDateString(undefined, {
                month: "short",
                day: "numeric",
              });
              const previewSnippet = note.content.replace(/[#*`_>\[\]]/g, "").trim() || "No content";

              return (
                <div
                  key={note.id}
                  onClick={() => selectNote(note)}
                  className={`p-3 cursor-pointer transition-colors relative group ${
                    isSelected
                      ? "bg-zinc-900/90 border-l-2 border-emerald-500"
                      : "hover:bg-zinc-900/50"
                  }`}
                >
                  <div className="flex items-start justify-between gap-1.5 mb-1">
                    <h3 className="text-xs font-semibold text-zinc-100 truncate flex-1">
                      {note.title || "Untitled Note"}
                    </h3>
                    <div className="flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button
                        onClick={(e) => handleTogglePin(e, note.id)}
                        className="text-zinc-500 hover:text-amber-400 p-0.5 rounded"
                        title={note.pinned ? "Unpin" : "Pin"}
                      >
                        {note.pinned ? (
                          <PinOff className="w-3 h-3 text-amber-400" />
                        ) : (
                          <Pin className="w-3 h-3" />
                        )}
                      </button>
                      <button
                        onClick={(e) => handleDeleteNote(e, note.id)}
                        className="text-zinc-500 hover:text-red-400 p-0.5 rounded"
                        title="Delete note"
                      >
                        <Trash2 className="w-3 h-3" />
                      </button>
                    </div>
                    {note.pinned === 1 && (
                      <Pin className="w-3 h-3 text-amber-400 flex-shrink-0 group-hover:hidden" />
                    )}
                  </div>

                  <p className="text-[11px] text-zinc-500 line-clamp-2 leading-relaxed mb-2">
                    {searchQuery.trim() ? (
                      <span>
                        {previewSnippet.split(new RegExp(`(${searchQuery.trim()})`, "gi")).map((part, i) =>
                          part.toLowerCase() === searchQuery.trim().toLowerCase() ? (
                            <mark key={i} className="bg-emerald-900/80 text-emerald-200 px-0.5 rounded">
                              {part}
                            </mark>
                          ) : (
                            part
                          )
                        )}
                      </span>
                    ) : (
                      previewSnippet
                    )}
                  </p>

                  <div className="flex items-center justify-between text-[10px] text-zinc-600">
                    <span className="flex items-center gap-1">
                      <Clock className="w-2.5 h-2.5" />
                      {dateStr}
                    </span>
                    <span className="bg-zinc-800 px-1.5 py-0.5 rounded text-zinc-400">
                      {note.notebook}
                    </span>
                  </div>
                </div>
              );
            })
          )}
        </div>
      </section>

      {/* ── Right Column: Editor & Preview Canvas / Graph View ── */}
      <main className="flex-1 bg-zinc-950 flex flex-col overflow-hidden">
        {editorMode === "graph" ? (
          <div className="flex-1 flex flex-col h-full">
            <header className="h-14 border-b border-zinc-800 px-6 flex items-center justify-between bg-zinc-950 shrink-0">
              <div className="flex items-center gap-2">
                <Network className="w-4 h-4 text-emerald-400" />
                <span className="font-semibold text-sm text-zinc-100">Knowledge Graph</span>
              </div>
              <button
                onClick={() => setEditorMode("split")}
                className="px-3 py-1.5 bg-zinc-800 hover:bg-zinc-700 text-xs rounded-md text-zinc-200 transition-colors"
              >
                Back to Editor
              </button>
            </header>
            <div className="flex-1 overflow-hidden">
              <GraphView
                notes={notes}
                activeNoteId={selectedNoteId}
                onSelectNote={(n) => {
                  selectNote(n);
                  setEditorMode("split");
                }}
              />
            </div>
          </div>
        ) : activeNote ? (
          <>
            {/* Header Toolbar */}
            <header className="h-14 border-b border-zinc-800 px-6 flex items-center justify-between bg-zinc-950 shrink-0">
              <div className="flex items-center gap-4 flex-1 mr-4">
                <input
                  type="text"
                  placeholder="Note Title..."
                  value={editTitle}
                  onChange={(e) => onEditChange({ title: e.target.value })}
                  className="bg-transparent text-lg font-bold text-white placeholder-zinc-700 focus:outline-none flex-1 truncate"
                />
              </div>

              <div className="flex items-center gap-3">
                {/* Auto-save Status */}
                <span className="text-[11px] text-zinc-500 flex items-center gap-1 font-mono">
                  {saveStatus === "saving" && "Saving..."}
                  {saveStatus === "saved" && (
                    <>
                      <Check className="w-3 h-3 text-emerald-400" />
                      <span>Saved</span>
                    </>
                  )}
                  {saveStatus === "unsaved" && "Unsaved"}
                </span>

                {/* View Mode Toggle */}
                <div className="flex items-center bg-zinc-900 border border-zinc-800 rounded-lg p-0.5">
                  <button
                    onClick={() => setEditorMode("edit")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "edit" ? "bg-zinc-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Editor Only"
                  >
                    <Edit3 className="w-3 h-3" />
                    <span>Edit</span>
                  </button>
                  <button
                    onClick={() => setEditorMode("split")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "split" ? "bg-zinc-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Split View"
                  >
                    <Columns className="w-3 h-3" />
                    <span>Split</span>
                  </button>
                  <button
                    onClick={() => setEditorMode("preview")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "preview" ? "bg-zinc-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Preview Only"
                  >
                    <Eye className="w-3 h-3" />
                    <span>Preview</span>
                  </button>
                  <button
                    onClick={() => setEditorMode("graph")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors text-zinc-500 hover:text-emerald-300`}
                    title="Graph View"
                  >
                    <Network className="w-3 h-3" />
                    <span>Graph</span>
                  </button>
                </div>

                {/* Export Dropdown */}
                <div className="relative">
                  <button
                    onClick={() => setShowExportMenu(!showExportMenu)}
                    className="p-1.5 rounded-lg bg-zinc-900 border border-zinc-800 hover:border-zinc-700 text-zinc-400 hover:text-zinc-200 text-xs flex items-center gap-1 transition-colors"
                    title="Export note"
                  >
                    <Download className="w-3.5 h-3.5" />
                  </button>
                  {showExportMenu && (
                    <div className="absolute right-0 mt-2 w-48 bg-zinc-900 border border-zinc-700 rounded-lg shadow-xl z-50 py-1 text-xs">
                      <button
                        onClick={exportAsMarkdown}
                        className="w-full text-left px-3 py-2 hover:bg-zinc-800 text-zinc-200 flex items-center gap-2"
                      >
                        <FileText className="w-3.5 h-3.5 text-emerald-400" />
                        <span>Export as Markdown (.md)</span>
                      </button>
                      <button
                        onClick={exportAsHtml}
                        className="w-full text-left px-3 py-2 hover:bg-zinc-800 text-zinc-200 flex items-center gap-2"
                      >
                        <ExternalLink className="w-3.5 h-3.5 text-sky-400" />
                        <span>Export as HTML (.html)</span>
                      </button>
                      <button
                        onClick={() => {
                          window.print();
                          setShowExportMenu(false);
                        }}
                        className="w-full text-left px-3 py-2 hover:bg-zinc-800 text-zinc-200 flex items-center gap-2 border-t border-zinc-800"
                      >
                        <Printer className="w-3.5 h-3.5 text-amber-400" />
                        <span>Print to PDF</span>
                      </button>
                    </div>
                  )}
                </div>
              </div>
            </header>

            {/* Split / Single Body Layout */}
            <div className="flex-1 flex overflow-hidden relative">
              {/* Left Side: Markdown Textarea Editor */}
              {(editorMode === "edit" || editorMode === "split") && (
                <div className="flex-1 flex flex-col p-6 overflow-hidden border-r border-zinc-800 relative">
                  <textarea
                    ref={textareaRef}
                    value={editContent}
                    onChange={(e) => onEditChange({ content: e.target.value })}
                    placeholder="Type your notes in Markdown. Use [[Note Title]] to link notes together..."
                    className="flex-1 w-full bg-transparent resize-none font-mono text-sm leading-relaxed text-zinc-200 placeholder-zinc-700 focus:outline-none"
                  />

                  {/* [[WikiLink]] Autocomplete Popup */}
                  {wikiSuggestOpen && (
                    <div className="absolute bottom-16 left-6 w-64 bg-zinc-900 border border-emerald-500/60 rounded-lg shadow-2xl p-1 z-30 max-h-48 overflow-y-auto">
                      <div className="px-2 py-1 text-[10px] font-semibold text-zinc-500 uppercase tracking-wider border-b border-zinc-800">
                        Link to Note
                      </div>
                      {notes
                        .filter(
                          (n) =>
                            n.id !== activeNote.id &&
                            n.title.toLowerCase().includes(wikiSuggestQuery.toLowerCase())
                        )
                        .slice(0, 5)
                        .map((n) => (
                          <button
                            key={n.id}
                            type="button"
                            onClick={() => insertWikilink(n.title)}
                            className="w-full text-left px-2 py-1.5 hover:bg-emerald-950/60 text-xs text-zinc-200 truncate flex items-center gap-1.5 rounded transition-colors"
                          >
                            <LinkIcon className="w-3 h-3 text-emerald-400 shrink-0" />
                            <span className="truncate">{n.title}</span>
                          </button>
                        ))}
                    </div>
                  )}

                  {/* Editor Bottom Meta Bar */}
                  <div className="pt-3 border-t border-zinc-900 flex items-center justify-between text-xs text-zinc-500 font-mono">
                    <div className="flex items-center gap-3">
                      <span>{wordCount} words</span>
                      <span>{editContent.length} chars</span>
                      <span>~{Math.max(1, Math.round(wordCount / 200))} min read</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <Tag className="w-3 h-3 text-zinc-600" />
                      <input
                        type="text"
                        placeholder="tags, separated, by, commas"
                        value={editTags}
                        onChange={(e) => onEditChange({ tags: e.target.value })}
                        className="bg-transparent text-xs text-zinc-400 placeholder-zinc-700 focus:outline-none text-right"
                      />
                    </div>
                  </div>
                </div>
              )}

              {/* Right Side: Live HTML Markdown Preview & Backlinks */}
              {(editorMode === "preview" || editorMode === "split") && (
                <div
                  className="flex-1 p-6 overflow-y-auto bg-zinc-950/50"
                  onClick={handlePreviewClick}
                >
                  <h1 className="text-2xl font-bold text-white mb-4 border-b border-zinc-800 pb-2">
                    {editTitle || "Untitled Note"}
                  </h1>

                  <div
                    className="prose prose-invert max-w-none text-sm text-zinc-200"
                    dangerouslySetInnerHTML={{ __html: renderMarkdown(editContent) }}
                  />

                  {/* Linked References / Backlinks Section */}
                  {backlinks.length > 0 && (
                    <div className="mt-8 pt-4 border-t border-zinc-800/80">
                      <div className="flex items-center gap-1.5 text-xs font-semibold text-zinc-400 uppercase tracking-wider mb-3">
                        <LinkIcon className="w-3.5 h-3.5 text-emerald-400" />
                        <span>Linked References ({backlinks.length})</span>
                      </div>
                      <div className="space-y-2">
                        {backlinks.map((bn) => (
                          <div
                            key={bn.id}
                            onClick={() => selectNote(bn)}
                            className="p-2.5 rounded-lg bg-zinc-900/60 hover:bg-zinc-900 border border-zinc-800/80 cursor-pointer transition-colors"
                          >
                            <div className="text-xs font-medium text-emerald-400 mb-1">
                              {bn.title}
                            </div>
                            <div className="text-[11px] text-zinc-500 line-clamp-1 font-mono">
                              {bn.content.replace(/[#*`_>\[\]]/g, "").slice(0, 100)}…
                            </div>
                          </div>
                        ))}
                      </div>
                    </div>
                  )}
                </div>
              )}
            </div>
          </>
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center p-8 text-center text-zinc-600">
            <BookOpen className="w-12 h-12 mb-3 text-zinc-800" />
            <p className="text-sm">Select a note or create a new one to begin editing</p>
          </div>
        )}
      </main>
    </div>
  );
}
