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

// Simple fast markdown parser for note preview
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
  const [editorMode, setEditorMode] = useState<"edit" | "split" | "preview">("split");

  // New Notebook Input Modal/State
  const [isAddingNotebook, setIsAddingNotebook] = useState(false);
  const [newNotebookName, setNewNotebookName] = useState("");

  const activeNote = useMemo(() => {
    return notes.find((n) => n.id === selectedNoteId) || null;
  }, [notes, selectedNoteId]);

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

  const handleCreateNote = async () => {
    try {
      const res = await fetch("/api/v1/notes", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          title: "Untitled Note",
          content: "# Untitled Note\n\nWrite your thoughts here...",
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
      } catch (err) {
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
    if (updates.content !== undefined) setEditContent(nextContent);
    if (updates.notebook !== undefined) setEditNotebook(nextNotebook);
    if (updates.tags !== undefined) setEditTags(nextTags);

    setSaveStatus("unsaved");

    if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    saveTimerRef.current = setTimeout(() => {
      saveCurrentNote(nextTitle, nextContent, nextNotebook, nextTags);
    }, 800);
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
    <div className="flex h-screen w-screen bg-nexus-950 font-sans text-zinc-100 antialiased overflow-hidden select-none">
      {/* ── Left Column: Notebooks & Filters ── */}
      <aside className="w-64 flex-shrink-0 bg-nexus-900 border-r border-nexus-800 flex flex-col justify-between">
        <div className="p-4 flex flex-col gap-4">
          <div className="flex items-center gap-2.5 px-2">
            <div className="w-8 h-8 rounded-lg bg-emerald-500/20 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
              <FileText className="w-4 h-4" />
            </div>
            <div>
              <h1 className="text-sm font-semibold tracking-wide text-zinc-100">Nexus Notes</h1>
              <p className="text-[11px] text-zinc-500">Sovereign Knowledge</p>
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
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <BookOpen className="w-3.5 h-3.5" />
                <span>All Notes</span>
              </div>
              <span className="text-[10px] bg-nexus-800 px-1.5 py-0.5 rounded text-zinc-400">
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
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <Pin className="w-3.5 h-3.5" />
                <span>Pinned</span>
              </div>
              <span className="text-[10px] bg-nexus-800 px-1.5 py-0.5 rounded text-zinc-400">
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
                className="text-zinc-400 hover:text-emerald-400 transition-colors p-1 rounded hover:bg-nexus-800"
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
                  className="w-full bg-nexus-950 border border-emerald-500/50 rounded-lg px-2.5 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:ring-1 focus:ring-emerald-500"
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
                      ? "bg-nexus-800 text-zinc-100 font-medium"
                      : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
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
        <div className="p-3 border-t border-nexus-800/80 bg-nexus-950/40 text-[11px] text-zinc-500 flex items-center justify-between">
          <div className="flex items-center gap-1.5">
            <ShieldCheck className="w-3.5 h-3.5 text-emerald-400" />
            <span>Sovereign Storage</span>
          </div>
          <span className="font-mono text-[10px] text-zinc-600">v1.0</span>
        </div>
      </aside>

      {/* ── Middle Column: Note Cards List ── */}
      <section className="w-72 flex-shrink-0 bg-nexus-950 border-r border-nexus-800 flex flex-col">
        {/* Search & New Note header */}
        <div className="p-3 border-b border-nexus-800 flex flex-col gap-2.5">
          <div className="flex items-center justify-between">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
              {selectedNotebook || (filterPinned ? "Pinned Notes" : "All Notes")}
            </h2>
            <button
              onClick={handleCreateNote}
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
              placeholder="Search notes..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full bg-nexus-900 border border-nexus-800 rounded-lg pl-8 pr-3 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:border-zinc-700"
            />
          </div>
        </div>

        {/* Notes Scrollable List */}
        <div className="flex-1 overflow-y-auto divide-y divide-nexus-850">
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
              return (
                <div
                  key={note.id}
                  onClick={() => selectNote(note)}
                  className={`p-3 cursor-pointer transition-colors relative group ${
                    isSelected
                      ? "bg-nexus-900/90 border-l-2 border-emerald-500"
                      : "hover:bg-nexus-900/50"
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
                    {note.content.replace(/[#*`_>\[\]]/g, "").trim() || "No content"}
                  </p>

                  <div className="flex items-center justify-between text-[10px] text-zinc-600">
                    <span className="flex items-center gap-1">
                      <Clock className="w-2.5 h-2.5" />
                      {dateStr}
                    </span>
                    <span className="bg-nexus-850 px-1.5 py-0.5 rounded text-zinc-400">
                      {note.notebook}
                    </span>
                  </div>
                </div>
              );
            })
          )}
        </div>
      </section>

      {/* ── Right Column: Editor & Preview Canvas ── */}
      <main className="flex-1 bg-nexus-950 flex flex-col overflow-hidden">
        {activeNote ? (
          <>
            {/* Header Toolbar */}
            <header className="h-14 border-b border-nexus-800 px-6 flex items-center justify-between bg-nexus-950">
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
                <div className="flex items-center bg-nexus-900 border border-nexus-800 rounded-lg p-0.5">
                  <button
                    onClick={() => setEditorMode("edit")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "edit" ? "bg-nexus-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Edit Only"
                  >
                    <Edit3 className="w-3 h-3" />
                  </button>
                  <button
                    onClick={() => setEditorMode("split")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "split" ? "bg-nexus-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Side by Side"
                  >
                    <Columns className="w-3 h-3" />
                  </button>
                  <button
                    onClick={() => setEditorMode("preview")}
                    className={`px-2 py-1 rounded text-xs flex items-center gap-1 transition-colors ${
                      editorMode === "preview" ? "bg-nexus-800 text-zinc-100" : "text-zinc-500 hover:text-zinc-300"
                    }`}
                    title="Preview Only"
                  >
                    <Eye className="w-3 h-3" />
                  </button>
                </div>
              </div>
            </header>

            {/* Note Metadata Sub-bar (Notebook & Tags) */}
            <div className="px-6 py-2 border-b border-nexus-800/60 bg-nexus-900/30 flex items-center gap-6 text-xs text-zinc-400">
              <div className="flex items-center gap-2">
                <Folder className="w-3.5 h-3.5 text-zinc-500" />
                <select
                  value={editNotebook}
                  onChange={(e) => onEditChange({ notebook: e.target.value })}
                  className="bg-transparent text-zinc-300 focus:outline-none cursor-pointer"
                >
                  <option value="General" className="bg-nexus-900">General</option>
                  {notebooks
                    .filter((nb) => nb.name !== "General")
                    .map((nb) => (
                      <option key={nb.id} value={nb.name} className="bg-nexus-900">
                        {nb.name}
                      </option>
                    ))}
                </select>
              </div>

              <div className="flex items-center gap-2 flex-1">
                <Tag className="w-3.5 h-3.5 text-zinc-500" />
                <input
                  type="text"
                  placeholder="Add tags (comma separated)..."
                  value={editTags}
                  onChange={(e) => onEditChange({ tags: e.target.value })}
                  className="bg-transparent placeholder-zinc-700 text-zinc-300 focus:outline-none flex-1"
                />
              </div>
            </div>

            {/* Editor & Preview Split Workspace */}
            <div className="flex-1 flex overflow-hidden">
              {(editorMode === "edit" || editorMode === "split") && (
                <div
                  className={`flex-1 flex flex-col border-r border-nexus-800/80 p-6 ${
                    editorMode === "edit" ? "w-full" : "w-1/2"
                  }`}
                >
                  <textarea
                    value={editContent}
                    onChange={(e) => onEditChange({ content: e.target.value })}
                    placeholder="Start writing markdown..."
                    className="w-full h-full bg-transparent resize-none font-mono text-sm leading-relaxed text-zinc-200 placeholder-zinc-700 focus:outline-none"
                  />
                </div>
              )}

              {(editorMode === "preview" || editorMode === "split") && (
                <div
                  className={`flex-1 overflow-y-auto p-6 bg-nexus-950/60 ${
                    editorMode === "preview" ? "w-full" : "w-1/2"
                  }`}
                >
                  <div
                    className="prose prose-invert max-w-none text-sm"
                    dangerouslySetInnerHTML={{
                      __html: renderMarkdown(editContent),
                    }}
                  />
                </div>
              )}
            </div>

            {/* Footer Metadata */}
            <footer className="h-8 border-t border-nexus-800 px-6 flex items-center justify-between text-[11px] text-zinc-600 bg-nexus-950">
              <div className="flex items-center gap-4">
                <span>{wordCount} words</span>
                <span>{editContent.length} characters</span>
              </div>
              <div>Markdown Enabled</div>
            </footer>
          </>
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center text-zinc-600 gap-3">
            <BookOpen className="w-10 h-10 stroke-1 text-zinc-700" />
            <p className="text-sm">Select a note or create a new one to begin editing</p>
            <button
              onClick={handleCreateNote}
              className="bg-emerald-500 hover:bg-emerald-400 text-zinc-950 font-medium text-xs px-4 py-2 rounded-lg transition-colors shadow-sm"
            >
              Create Note
            </button>
          </div>
        )}
      </main>
    </div>
  );
}
