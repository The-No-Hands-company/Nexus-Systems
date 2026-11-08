import React, { useState, useEffect, useCallback, useMemo, useRef } from "react";
import {
  FileText,
  Folder,
  Plus,
  Search,
  Trash2,
  Star,
  Pin,
  Check,
  ChevronRight,
  ChevronDown,
  Sparkles,
  Heading1,
  Heading2,
  Heading3,
  List,
  ListOrdered,
  CheckSquare,
  Quote,
  Code,
  Share2,
  Download,
  Shield,
  Layers,
} from "lucide-react";

interface Document {
  id: string;
  title: string;
  content: string;
  folder: string;
  icon: string;
  coverUrl: string;
  pinned: number;
  starred: number;
  createdAt: string;
  updatedAt: string;
}

interface FolderItem {
  id: string;
  name: string;
  color: string;
  icon: string;
  createdAt: string;
}

const EMOJI_OPTIONS = ["📄", "📘", "💡", "⚡", "🚀", "🔥", "🎯", "🛠️", "📚", "📊", "🔒", "🌐"];

// Fast document formatter
function renderDocHtml(content: string): string {
  if (!content) return '<p class="text-zinc-600 italic">Empty document. Type or use the toolbar above...</p>';
  let html = content
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");

  // Callouts
  html = html.replace(/^:::callout\s*(.*)\n([\s\S]*?):::/gm, (_, title, text) => {
    return `<div class="bg-indigo-950/40 border border-indigo-500/30 rounded-xl p-4 my-4 flex gap-3 text-indigo-200"><span class="text-xl">💡</span><div><div class="font-semibold text-indigo-300 mb-1">${title}</div><div class="text-sm text-indigo-200/90 leading-relaxed">${text}</div></div></div>`;
  });

  // Code blocks
  html = html.replace(/```([a-z0-9_-]*)\n([\s\S]*?)```/g, (_, lang, code) => {
    return `<pre class="bg-zinc-900 border border-zinc-800 p-4 rounded-xl overflow-x-auto text-sm my-4 font-mono text-indigo-300"><code>${code}</code></pre>`;
  });

  // Inline code
  html = html.replace(/`([^`]+)`/g, '<code class="bg-zinc-800 text-indigo-300 px-1.5 py-0.5 rounded text-xs font-mono">$1</code>');

  // Headings
  html = html.replace(/^### (.*$)/gim, '<h3 class="text-lg font-bold text-zinc-100 mt-6 mb-2">$1</h3>');
  html = html.replace(/^## (.*$)/gim, '<h2 class="text-xl font-bold text-zinc-100 mt-7 mb-3 pb-1 border-b border-zinc-800">$1</h2>');
  html = html.replace(/^# (.*$)/gim, '<h1 class="text-3xl font-extrabold text-white mt-8 mb-4 pb-2 border-b border-zinc-800">$1</h1>');

  // Quotes
  html = html.replace(/^> (.*$)/gim, '<blockquote class="border-l-4 border-indigo-500/80 pl-4 py-1 italic text-zinc-300 my-3 bg-zinc-900/40 rounded-r-lg">$1</blockquote>');

  // Checkboxes
  html = html.replace(/^- \[ \] (.*$)/gim, '<div class="flex items-center gap-2.5 my-1.5"><input type="checkbox" disabled class="w-4 h-4 rounded border-zinc-700 text-indigo-600"><span class="text-zinc-200 text-sm">$1</span></div>');
  html = html.replace(/^- \[x\] (.*$)/gim, '<div class="flex items-center gap-2.5 my-1.5"><input type="checkbox" checked disabled class="w-4 h-4 rounded border-zinc-700 text-indigo-600"><span class="text-zinc-500 line-through text-sm">$1</span></div>');

  // Lists
  html = html.replace(/^\* (.*$)/gim, '<li class="ml-5 list-disc text-zinc-300 my-1">$1</li>');
  html = html.replace(/^- (.*$)/gim, '<li class="ml-5 list-disc text-zinc-300 my-1">$1</li>');

  // Bold & Italic
  html = html.replace(/\*\*([^*]+)\*\*/g, '<strong class="font-bold text-zinc-100">$1</strong>');
  html = html.replace(/\*([^*]+)\*/g, '<em class="italic text-zinc-300">$1</em>');

  // Paragraphs
  html = html.replace(/\n\n+/g, '</p><p class="my-3 leading-relaxed text-zinc-300">');
  return `<p class="my-3 leading-relaxed text-zinc-300">${html}</p>`;
}

export default function App() {
  const [folders, setFolders] = useState<FolderItem[]>([]);
  const [documents, setDocuments] = useState<Document[]>([]);
  const [selectedFolder, setSelectedFolder] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [activeDocId, setActiveDocId] = useState<string | null>(null);
  const [filterStarred, setFilterStarred] = useState(false);

  // Editor states
  const [docTitle, setDocTitle] = useState("");
  const [docContent, setDocContent] = useState("");
  const [docIcon, setDocIcon] = useState("📄");
  const [docFolder, setDocFolder] = useState("General");
  const [showEmojiPicker, setShowEmojiPicker] = useState(false);
  const [saveStatus, setSaveStatus] = useState<"saved" | "saving" | "unsaved">("saved");

  // New folder input
  const [isAddingFolder, setIsAddingFolder] = useState(false);
  const [newFolderName, setNewFolderName] = useState("");

  const activeDocIdRef = useRef<string | null>(activeDocId);
  activeDocIdRef.current = activeDocId;

  const activeDoc = useMemo(() => {
    return documents.find((d) => d.id === activeDocId) || null;
  }, [documents, activeDocId]);

  const loadFolders = useCallback(async () => {
    try {
      const res = await fetch("/api/v1/folders");
      if (res.ok) {
        const data = await res.json();
        setFolders(data);
      }
    } catch (err) {
      console.error("Failed to load folders", err);
    }
  }, []);

  const loadDocuments = useCallback(async () => {
    try {
      let url = "/api/v1/documents";
      if (searchQuery.trim()) {
        url = `/api/v1/documents/search?q=${encodeURIComponent(searchQuery.trim())}`;
      } else if (selectedFolder) {
        url = `/api/v1/documents?folder=${encodeURIComponent(selectedFolder)}`;
      }
      const res = await fetch(url);
      if (res.ok) {
        const data: Document[] = await res.json();
        setDocuments(data);
        if (data.length > 0 && !activeDocIdRef.current) {
          selectDocument(data[0]);
        }
      }
    } catch (err) {
      console.error("Failed to load documents", err);
    }
  }, [selectedFolder, searchQuery]);

  useEffect(() => {
    loadFolders();
  }, [loadFolders]);

  useEffect(() => {
    loadDocuments();
  }, [loadDocuments]);

  const selectDocument = (doc: Document) => {
    setActiveDocId(doc.id);
    setDocTitle(doc.title);
    setDocContent(doc.content);
    setDocIcon(doc.icon || "📄");
    setDocFolder(doc.folder || "General");
    setSaveStatus("saved");
  };

  const createNewDocument = async () => {
    try {
      const res = await fetch("/api/v1/documents", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          title: "Untitled Document",
          content: "# Untitled Document\n\nStart typing your document...",
          folder: selectedFolder || "General",
          icon: "📄",
        }),
      });
      if (res.ok) {
        const newDoc: Document = await res.json();
        setDocuments((prev) => [newDoc, ...prev]);
        selectDocument(newDoc);
      }
    } catch (err) {
      console.error("Failed to create document", err);
    }
  };

  const handleCreateFolder = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newFolderName.trim()) return;
    try {
      const res = await fetch("/api/v1/folders", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: newFolderName.trim() }),
      });
      if (res.ok) {
        setNewFolderName("");
        setIsAddingFolder(false);
        loadFolders();
      }
    } catch (err) {
      console.error("Failed to create folder", err);
    }
  };

  const saveTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const saveDocument = useCallback(
    async (title: string, content: string, folder: string, icon: string) => {
      if (!activeDocId) return;
      setSaveStatus("saving");
      try {
        const res = await fetch(`/api/v1/documents/${activeDocId}`, {
          method: "PATCH",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ title, content, folder, icon }),
        });
        if (res.ok) {
          const updated: Document = await res.json();
          setDocuments((prev) => prev.map((d) => (d.id === updated.id ? updated : d)));
          setSaveStatus("saved");
        } else {
          setSaveStatus("unsaved");
        }
      } catch (err) {
        setSaveStatus("unsaved");
      }
    },
    [activeDocId]
  );

  const onDocChange = (updates: {
    title?: string;
    content?: string;
    folder?: string;
    icon?: string;
  }) => {
    const nextTitle = updates.title !== undefined ? updates.title : docTitle;
    const nextContent = updates.content !== undefined ? updates.content : docContent;
    const nextFolder = updates.folder !== undefined ? updates.folder : docFolder;
    const nextIcon = updates.icon !== undefined ? updates.icon : docIcon;

    if (updates.title !== undefined) setDocTitle(nextTitle);
    if (updates.content !== undefined) setDocContent(nextContent);
    if (updates.folder !== undefined) setDocFolder(nextFolder);
    if (updates.icon !== undefined) setDocIcon(nextIcon);

    setSaveStatus("unsaved");

    if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    saveTimerRef.current = setTimeout(() => {
      saveDocument(nextTitle, nextContent, nextFolder, nextIcon);
    }, 800);
  };

  const toggleStar = async (e: React.MouseEvent, docId: string, currentStarred: number) => {
    e.stopPropagation();
    try {
      const res = await fetch(`/api/v1/documents/${docId}`, {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ starred: currentStarred ? 0 : 1 }),
      });
      if (res.ok) {
        const updated: Document = await res.json();
        setDocuments((prev) => prev.map((d) => (d.id === updated.id ? updated : d)));
      }
    } catch (err) {
      console.error("Failed to toggle star", err);
    }
  };

  const deleteDoc = async (e: React.MouseEvent, docId: string) => {
    e.stopPropagation();
    if (!confirm("Are you sure you want to delete this document?")) return;
    try {
      const res = await fetch(`/api/v1/documents/${docId}`, { method: "DELETE" });
      if (res.ok) {
        setDocuments((prev) => prev.filter((d) => d.id !== docId));
        if (activeDocId === docId) {
          const remaining = documents.filter((d) => d.id !== docId);
          if (remaining.length > 0) selectDocument(remaining[0]);
          else setActiveDocId(null);
        }
      }
    } catch (err) {
      console.error("Failed to delete document", err);
    }
  };

  // Quick formatting toolbar insertion
  const insertFormatting = (prefix: string, suffix = "") => {
    const textarea = document.getElementById("doc-textarea") as HTMLTextAreaElement;
    if (!textarea) return;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const text = textarea.value;
    const selected = text.substring(start, end) || "text";
    const replacement = `${prefix}${selected}${suffix}`;
    const nextContent = text.substring(0, start) + replacement + text.substring(end);
    onDocChange({ content: nextContent });
    setTimeout(() => {
      textarea.focus();
      textarea.setSelectionRange(start + prefix.length, start + prefix.length + selected.length);
    }, 10);
  };

  const displayedDocs = useMemo(() => {
    let result = documents;
    if (filterStarred) {
      result = result.filter((d) => d.starred === 1);
    }
    return result;
  }, [documents, filterStarred]);

  return (
    <div className="flex h-screen w-screen bg-nexus-950 font-sans text-zinc-100 antialiased overflow-hidden select-none">
      {/* ── Left Sidebar: Folders & Workspaces ── */}
      <aside className="w-64 flex-shrink-0 bg-nexus-900 border-r border-nexus-800 flex flex-col justify-between">
        <div className="p-4 flex flex-col gap-4">
          <div className="flex items-center gap-2.5 px-2">
            <div className="w-8 h-8 rounded-lg bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400">
              <FileText className="w-4 h-4" />
            </div>
            <div>
              <h1 className="text-sm font-semibold tracking-wide text-zinc-100">Nexus Docs</h1>
              <p className="text-[11px] text-zinc-500">Document Workspace</p>
            </div>
          </div>

          <div className="flex flex-col gap-1 text-xs">
            <button
              onClick={() => {
                setSelectedFolder(null);
                setFilterStarred(false);
              }}
              className={`flex items-center justify-between px-3 py-2 rounded-lg transition-colors ${
                !selectedFolder && !filterStarred
                  ? "bg-indigo-500/15 text-indigo-300 font-medium border border-indigo-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <Layers className="w-3.5 h-3.5" />
                <span>All Documents</span>
              </div>
              <span className="text-[10px] bg-nexus-800 px-1.5 py-0.5 rounded text-zinc-400">
                {documents.length}
              </span>
            </button>

            <button
              onClick={() => {
                setSelectedFolder(null);
                setFilterStarred(true);
              }}
              className={`flex items-center justify-between px-3 py-2 rounded-lg transition-colors ${
                filterStarred
                  ? "bg-amber-500/15 text-amber-300 font-medium border border-amber-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <div className="flex items-center gap-2.5">
                <Star className="w-3.5 h-3.5" />
                <span>Starred</span>
              </div>
              <span className="text-[10px] bg-nexus-800 px-1.5 py-0.5 rounded text-zinc-400">
                {documents.filter((d) => d.starred === 1).length}
              </span>
            </button>
          </div>

          {/* Folders Section */}
          <div className="pt-2">
            <div className="flex items-center justify-between px-2 mb-2">
              <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-500">
                Folders
              </span>
              <button
                onClick={() => setIsAddingFolder(true)}
                className="text-zinc-400 hover:text-indigo-400 transition-colors p-1 rounded hover:bg-nexus-800"
                title="Create Folder"
              >
                <Plus className="w-3.5 h-3.5" />
              </button>
            </div>

            {isAddingFolder && (
              <form onSubmit={handleCreateFolder} className="mb-2 px-1">
                <input
                  type="text"
                  autoFocus
                  placeholder="Folder name..."
                  value={newFolderName}
                  onChange={(e) => setNewFolderName(e.target.value)}
                  onBlur={() => {
                    if (!newFolderName.trim()) setIsAddingFolder(false);
                  }}
                  className="w-full bg-nexus-950 border border-indigo-500/50 rounded-lg px-2.5 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:ring-1 focus:ring-indigo-500"
                />
              </form>
            )}

            <div className="flex flex-col gap-0.5 max-h-56 overflow-y-auto">
              {folders.map((f) => (
                <button
                  key={f.id}
                  onClick={() => {
                    setSelectedFolder(f.name);
                    setFilterStarred(false);
                  }}
                  className={`flex items-center justify-between px-3 py-1.5 rounded-lg text-xs transition-colors ${
                    selectedFolder === f.name
                      ? "bg-nexus-800 text-zinc-100 font-medium"
                      : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
                  }`}
                >
                  <div className="flex items-center gap-2 truncate">
                    <Folder className="w-3 h-3 text-indigo-400 flex-shrink-0" />
                    <span className="truncate">{f.name}</span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        </div>

        {/* Footer Identity */}
        <div className="p-3 border-t border-nexus-800/80 bg-nexus-950/40 text-[11px] text-zinc-500 flex items-center justify-between">
          <div className="flex items-center gap-1.5">
            <Shield className="w-3.5 h-3.5 text-indigo-400" />
            <span>Encrypted Docs</span>
          </div>
          <span className="font-mono text-[10px] text-zinc-600">v1.0</span>
        </div>
      </aside>

      {/* ── Document List ── */}
      <section className="w-72 flex-shrink-0 bg-nexus-950 border-r border-nexus-800 flex flex-col">
        <div className="p-3 border-b border-nexus-800 flex flex-col gap-2.5">
          <div className="flex items-center justify-between">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
              {selectedFolder || (filterStarred ? "Starred" : "All Documents")}
            </h2>
            <button
              onClick={createNewDocument}
              className="flex items-center gap-1 bg-indigo-600 hover:bg-indigo-500 text-white font-medium text-xs px-2.5 py-1 rounded-md transition-colors shadow-sm"
            >
              <Plus className="w-3.5 h-3.5" />
              <span>Doc</span>
            </button>
          </div>

          <div className="relative">
            <Search className="w-3.5 h-3.5 text-zinc-500 absolute left-2.5 top-2.5 pointer-events-none" />
            <input
              type="text"
              placeholder="Search documents..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full bg-nexus-900 border border-nexus-800 rounded-lg pl-8 pr-3 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none focus:border-zinc-700"
            />
          </div>
        </div>

        <div className="flex-1 overflow-y-auto divide-y divide-nexus-850">
          {displayedDocs.length === 0 ? (
            <div className="p-8 text-center text-xs text-zinc-600">
              No documents found. Create one!
            </div>
          ) : (
            displayedDocs.map((doc) => {
              const isSelected = doc.id === activeDocId;
              const dateStr = new Date(doc.updatedAt).toLocaleDateString(undefined, {
                month: "short",
                day: "numeric",
              });
              return (
                <div
                  key={doc.id}
                  onClick={() => selectDocument(doc)}
                  className={`p-3 cursor-pointer transition-colors relative group ${
                    isSelected
                      ? "bg-nexus-900/90 border-l-2 border-indigo-500"
                      : "hover:bg-nexus-900/50"
                  }`}
                >
                  <div className="flex items-start justify-between gap-1.5 mb-1">
                    <div className="flex items-center gap-1.5 truncate flex-1">
                      <span className="text-sm">{doc.icon || "📄"}</span>
                      <h3 className="text-xs font-semibold text-zinc-100 truncate">
                        {doc.title || "Untitled Document"}
                      </h3>
                    </div>
                    <div className="flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button
                        onClick={(e) => toggleStar(e, doc.id, doc.starred)}
                        className="text-zinc-500 hover:text-amber-400 p-0.5 rounded"
                        title={doc.starred ? "Unstar" : "Star"}
                      >
                        <Star
                          className={`w-3 h-3 ${
                            doc.starred ? "text-amber-400 fill-amber-400" : ""
                          }`}
                        />
                      </button>
                      <button
                        onClick={(e) => deleteDoc(e, doc.id)}
                        className="text-zinc-500 hover:text-red-400 p-0.5 rounded"
                        title="Delete document"
                      >
                        <Trash2 className="w-3 h-3" />
                      </button>
                    </div>
                    {doc.starred === 1 && (
                      <Star className="w-3 h-3 text-amber-400 fill-amber-400 flex-shrink-0 group-hover:hidden" />
                    )}
                  </div>

                  <p className="text-[11px] text-zinc-500 line-clamp-2 leading-relaxed mb-2">
                    {doc.content.replace(/[#*`_>\[\]]/g, "").trim() || "No content"}
                  </p>

                  <div className="flex items-center justify-between text-[10px] text-zinc-600">
                    <span>{dateStr}</span>
                    <span className="bg-nexus-850 px-1.5 py-0.5 rounded text-zinc-400">
                      {doc.folder}
                    </span>
                  </div>
                </div>
              );
            })
          )}
        </div>
      </section>

      {/* ── Document Canvas ── */}
      <main className="flex-1 bg-nexus-950 flex flex-col overflow-hidden">
        {activeDoc ? (
          <>
            {/* Header Toolbar */}
            <header className="h-14 border-b border-nexus-800 px-6 flex items-center justify-between bg-nexus-950">
              <div className="flex items-center gap-3">
                {/* Icon selector */}
                <div className="relative">
                  <button
                    onClick={() => setShowEmojiPicker(!showEmojiPicker)}
                    className="w-8 h-8 rounded-lg bg-nexus-900 border border-nexus-800 flex items-center justify-center text-lg hover:bg-nexus-800 transition-colors"
                  >
                    {docIcon}
                  </button>
                  {showEmojiPicker && (
                    <div className="absolute top-10 left-0 bg-nexus-900 border border-nexus-800 rounded-xl p-2 shadow-2xl grid grid-cols-4 gap-1 z-50">
                      {EMOJI_OPTIONS.map((emoji) => (
                        <button
                          key={emoji}
                          onClick={() => {
                            onDocChange({ icon: emoji });
                            setShowEmojiPicker(false);
                          }}
                          className="w-7 h-7 flex items-center justify-center rounded hover:bg-nexus-800 text-sm"
                        >
                          {emoji}
                        </button>
                      ))}
                    </div>
                  )}
                </div>

                <div className="flex items-center gap-2">
                  <Folder className="w-3.5 h-3.5 text-zinc-500" />
                  <select
                    value={docFolder}
                    onChange={(e) => onDocChange({ folder: e.target.value })}
                    className="bg-transparent text-xs text-zinc-300 focus:outline-none cursor-pointer"
                  >
                    <option value="General" className="bg-nexus-900">General</option>
                    {folders
                      .filter((f) => f.name !== "General")
                      .map((f) => (
                        <option key={f.id} value={f.name} className="bg-nexus-900">
                          {f.name}
                        </option>
                      ))}
                  </select>
                </div>
              </div>

              {/* Formatting Toolbar */}
              <div className="flex items-center gap-1 bg-nexus-900 border border-nexus-800 rounded-lg p-1 text-zinc-400">
                <button
                  onClick={() => insertFormatting("# ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Heading 1"
                >
                  <Heading1 className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("## ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Heading 2"
                >
                  <Heading2 className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("### ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Heading 3"
                >
                  <Heading3 className="w-3.5 h-3.5" />
                </button>
                <div className="w-[1px] h-4 bg-nexus-800 mx-1" />
                <button
                  onClick={() => insertFormatting("**", "**")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800 font-bold text-xs px-1.5"
                  title="Bold"
                >
                  B
                </button>
                <button
                  onClick={() => insertFormatting("*", "*")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800 italic text-xs px-1.5"
                  title="Italic"
                >
                  I
                </button>
                <button
                  onClick={() => insertFormatting("- ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Bullet List"
                >
                  <List className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("- [ ] ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Task Checklist"
                >
                  <CheckSquare className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("> ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Blockquote"
                >
                  <Quote className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("```\n", "\n```")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Code Block"
                >
                  <Code className="w-3.5 h-3.5" />
                </button>
              </div>

              {/* Status and Action */}
              <div className="flex items-center gap-3">
                <span className="text-[11px] text-zinc-500 flex items-center gap-1 font-mono">
                  {saveStatus === "saving" && "Saving..."}
                  {saveStatus === "saved" && (
                    <>
                      <Check className="w-3 h-3 text-indigo-400" />
                      <span>Saved</span>
                    </>
                  )}
                  {saveStatus === "unsaved" && "Unsaved"}
                </span>
              </div>
            </header>

            {/* Document Body: Split Editor & Preview Canvas */}
            <div className="flex-1 flex overflow-hidden">
              {/* Left Edit Pane */}
              <div className="flex-1 flex flex-col border-r border-nexus-800/80 p-8 overflow-y-auto">
                <input
                  type="text"
                  placeholder="Document Title"
                  value={docTitle}
                  onChange={(e) => onDocChange({ title: e.target.value })}
                  className="bg-transparent text-3xl font-extrabold text-white placeholder-zinc-700 focus:outline-none mb-6"
                />

                <textarea
                  id="doc-textarea"
                  value={docContent}
                  onChange={(e) => onDocChange({ content: e.target.value })}
                  placeholder="Type your markdown document..."
                  className="w-full flex-1 bg-transparent resize-none font-mono text-sm leading-relaxed text-zinc-200 placeholder-zinc-700 focus:outline-none"
                />
              </div>

              {/* Right Live Rendered Pane */}
              <div className="flex-1 p-8 overflow-y-auto bg-nexus-950/40">
                <div className="max-w-2xl mx-auto">
                  <h1 className="text-3xl font-extrabold text-white mb-6 pb-2 border-b border-nexus-800">
                    {docTitle || "Untitled Document"}
                  </h1>
                  <div
                    className="prose prose-invert max-w-none text-sm"
                    dangerouslySetInnerHTML={{
                      __html: renderDocHtml(docContent),
                    }}
                  />
                </div>
              </div>
            </div>
          </>
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center text-zinc-600 gap-3">
            <FileText className="w-10 h-10 stroke-1 text-zinc-700" />
            <p className="text-sm">Select a document or create a new one to begin editing</p>
            <button
              onClick={createNewDocument}
              className="bg-indigo-600 hover:bg-indigo-500 text-white font-medium text-xs px-4 py-2 rounded-lg transition-colors shadow-sm"
            >
              Create Document
            </button>
          </div>
        )}
      </main>
    </div>
  );
}
