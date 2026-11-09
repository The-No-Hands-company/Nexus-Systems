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
  Table,
  History,
  Clock,
  Printer,
  RotateCcw,
  FileDown,
  X,
  Camera,
  Image,
  Palette,
  Users,
  Wifi,
  WifiOff,
  Copy,
  Minus,
  Info,
  AlertTriangle,
  CheckCircle2,
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

interface DocumentRevision {
  id: string;
  documentId: string;
  title: string;
  content: string;
  createdAt: string;
}

interface FolderItem {
  id: string;
  name: string;
  color: string;
  icon: string;
  createdAt: string;
}

interface PeerPresence {
  peerId: string;
  name: string;
  color: string;
}

const PEER_COLORS = [
  "#6366f1", // Indigo
  "#10b981", // Emerald
  "#f59e0b", // Amber
  "#ec4899", // Pink
  "#06b6d4", // Cyan
  "#8b5cf6", // Purple
];

const COVER_PRESETS = [
  { name: "Obsidian Slate", style: "linear-gradient(135deg, #09090b 0%, #18181b 50%, #27272a 100%)" },
  { name: "Cosmic Indigo", style: "linear-gradient(135deg, #1e1b4b 0%, #4c1d95 50%, #701a75 100%)" },
  { name: "Emerald Aurora", style: "linear-gradient(135deg, #022c22 0%, #065f46 50%, #047857 100%)" },
  { name: "Midnight Oceanic", style: "linear-gradient(135deg, #082f49 0%, #0369a1 50%, #1d4ed8 100%)" },
  { name: "Sunset Horizon", style: "linear-gradient(135deg, #4c0519 0%, #9f1239 50%, #ea580c 100%)" },
  { name: "Cyber Teal", style: "linear-gradient(135deg, #042f2e 0%, #0d9488 50%, #0369a1 100%)" },
];

const EMOJI_CATEGORIES = [
  {
    name: "Documents & Work",
    emojis: ["📄", "📝", "📋", "📁", "📂", "📊", "📈", "📅", "📌", "📎", "🏷️", "💼", "🏢", "📑"],
  },
  {
    name: "Technology & Code",
    emojis: ["🚀", "⚡", "🔥", "💡", "🛠️", "⚙️", "🔒", "🌐", "💻", "🤖", "📦", "🧠", "🔌", "📡"],
  },
  {
    name: "Creative & Design",
    emojis: ["🎨", "✨", "🎯", "💎", "🔮", "🌈", "⭐", "🏆", "🌟", "🎪", "🎭", "🖌️", "📷", "🪄"],
  },
  {
    name: "Knowledge & Notes",
    emojis: ["📚", "📖", "🔬", "🧪", "🧬", "🔭", "📐", "📜", "🎓", "☕", "🌿", "🌱", "🌊", "🌙"],
  },
];

// Markdown table generator & parser
function renderTableHtml(rows: string[][]): string {
  if (rows.length === 0) return "";
  const header = rows[0];
  const body = rows.slice(1);
  const headerHtml = `<thead><tr class="border-b border-zinc-800 bg-zinc-900/90">${header
    .map((h) => `<th class="px-4 py-2 text-left text-xs font-semibold text-zinc-200 border-r border-zinc-800 last:border-r-0">${h}</th>`)
    .join("")}</tr></thead>`;
  const bodyHtml = `<tbody>${body
    .map(
      (r) =>
        `<tr class="border-b border-zinc-850 hover:bg-zinc-900/40 transition-colors">${r
          .map((c) => `<td class="px-4 py-2 text-xs text-zinc-300 border-r border-zinc-850 last:border-r-0 font-sans">${c}</td>`)
          .join("")}</tr>`
    )
    .join("")}</tbody>`;
  return `<div class="overflow-x-auto my-4 rounded-xl border border-zinc-800 bg-zinc-950/60 shadow-inner"><table class="w-full text-left border-collapse">${headerHtml}${bodyHtml}</table></div>`;
}

function parseMarkdownTables(text: string): string {
  const lines = text.split("\n");
  const result: string[] = [];
  let inTable = false;
  let tableRows: string[][] = [];

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (line.startsWith("|") && line.endsWith("|")) {
      const cells = line
        .slice(1, -1)
        .split("|")
        .map((c) => c.trim());
      // Check for separator row: | --- | :---: | ---: |
      if (cells.every((c) => /^:?-+:?$/.test(c))) {
        continue;
      }
      if (!inTable) {
        inTable = true;
        tableRows = [];
      }
      tableRows.push(cells);
    } else {
      if (inTable) {
        result.push(renderTableHtml(tableRows));
        inTable = false;
        tableRows = [];
      }
      result.push(lines[i]);
    }
  }
  if (inTable) {
    result.push(renderTableHtml(tableRows));
  }
  return result.join("\n");
}

function renderDocHtml(content: string): string {
  if (!content) return '<p class="text-zinc-600 italic">Empty document. Type or use the toolbar above...</p>';

  // Extract & render tables
  const processed = parseMarkdownTables(content);

  let html = processed
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");

  // Re-allow the HTML tables we generated safely
  html = html.replace(/&lt;div class="overflow-x-auto([\s\S]*?)&lt;\/div&gt;/g, (m) => {
    return m
      .replace(/&lt;/g, "<")
      .replace(/&gt;/g, ">")
      .replace(/&quot;/g, '"');
  });

  // Callouts: :::tip, :::warning, :::danger, :::info, :::callout
  html = html.replace(/^:::tip\s*(.*)\n([\s\S]*?):::/gm, (_, title, text) => {
    return `<div class="bg-emerald-950/40 border border-emerald-500/30 rounded-xl p-4 my-4 flex gap-3 text-emerald-200"><span class="text-xl">💡</span><div><div class="font-semibold text-emerald-300 mb-1">${title || "Tip"}</div><div class="text-sm text-emerald-200/90 leading-relaxed">${text}</div></div></div>`;
  });

  html = html.replace(/^:::warning\s*(.*)\n([\s\S]*?):::/gm, (_, title, text) => {
    return `<div class="bg-amber-950/40 border border-amber-500/30 rounded-xl p-4 my-4 flex gap-3 text-amber-200"><span class="text-xl">⚠️</span><div><div class="font-semibold text-amber-300 mb-1">${title || "Warning"}</div><div class="text-sm text-amber-200/90 leading-relaxed">${text}</div></div></div>`;
  });

  html = html.replace(/^:::danger\s*(.*)\n([\s\S]*?):::/gm, (_, title, text) => {
    return `<div class="bg-rose-950/40 border border-rose-500/30 rounded-xl p-4 my-4 flex gap-3 text-rose-200"><span class="text-xl">🛑</span><div><div class="font-semibold text-rose-300 mb-1">${title || "Caution"}</div><div class="text-sm text-rose-200/90 leading-relaxed">${text}</div></div></div>`;
  });

  html = html.replace(/^:::(?:info|callout)\s*(.*)\n([\s\S]*?):::/gm, (_, title, text) => {
    return `<div class="bg-indigo-950/40 border border-indigo-500/30 rounded-xl p-4 my-4 flex gap-3 text-indigo-200"><span class="text-xl">ℹ️</span><div><div class="font-semibold text-indigo-300 mb-1">${title || "Note"}</div><div class="text-sm text-indigo-200/90 leading-relaxed">${text}</div></div></div>`;
  });

  // Dividers
  html = html.replace(/^(?:---|___|\*\*\*)\s*$/gm, '<hr class="my-6 border-zinc-800" />');

  // Code blocks
  html = html.replace(/```([a-z0-9_-]*)\n([\s\S]*?)```/g, (_, _lang, code) => {
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

  // Strikethrough
  html = html.replace(/~~([^~]+)~~/g, '<del class="line-through text-zinc-500">$1</del>');

  // Lists
  html = html.replace(/^\* (.*$)/gim, '<li class="ml-5 list-disc text-zinc-300 my-1">$1</li>');
  html = html.replace(/^- (.*$)/gim, '<li class="ml-5 list-disc text-zinc-300 my-1">$1</li>');
  html = html.replace(/^\d+\. (.*$)/gim, '<li class="ml-5 list-decimal text-zinc-300 my-1">$1</li>');

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
  const [docCover, setDocCover] = useState("");
  const [customCoverUrl, setCustomCoverUrl] = useState("");
  const [saveStatus, setSaveStatus] = useState<"saved" | "saving" | "unsaved">("saved");

  // Modals & Panels
  const [showEmojiPicker, setShowEmojiPicker] = useState(false);
  const [emojiSearch, setEmojiSearch] = useState("");
  const [showCoverPicker, setShowCoverPicker] = useState(false);
  const [showExportMenu, setShowExportMenu] = useState(false);
  const [showSlashMenu, setShowSlashMenu] = useState(false);
  const [slashMenuFilter, setSlashMenuFilter] = useState("");
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  // Revisions & History Drawer
  const [showRevisions, setShowRevisions] = useState(false);
  const [revisions, setRevisions] = useState<DocumentRevision[]>([]);
  const [loadingRevisions, setLoadingRevisions] = useState(false);
  const [selectedRevision, setSelectedRevision] = useState<DocumentRevision | null>(null);

  // Collaboration state
  const [connectedPeers, setConnectedPeers] = useState<PeerPresence[]>([]);
  const [isCollabConnected, setIsCollabConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const myPeerId = useMemo(() => Math.random().toString(36).slice(2, 9), []);
  const myPeerName = useMemo(() => `User-${myPeerId.slice(0, 4)}`, [myPeerId]);

  // New folder input
  const [isAddingFolder, setIsAddingFolder] = useState(false);
  const [newFolderName, setNewFolderName] = useState("");

  const activeDocIdRef = useRef<string | null>(activeDocId);
  activeDocIdRef.current = activeDocId;

  const activeDoc = useMemo(() => {
    return documents.find((d) => d.id === activeDocId) || null;
  }, [documents, activeDocId]);

  const metrics = useMemo(() => {
    const text = docContent.trim();
    const words = text ? text.split(/\s+/).filter(Boolean).length : 0;
    const chars = text.length;
    const readTimeMinutes = Math.max(1, Math.ceil(words / 200));
    return { words, chars, readTimeMinutes };
  }, [docContent]);

  const showToast = (msg: string) => {
    setToastMessage(msg);
    setTimeout(() => setToastMessage(null), 3000);
  };

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

  // Real-time Collaborative Sync via WebSockets
  useEffect(() => {
    if (!activeDocId) {
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
      setIsCollabConnected(false);
      setConnectedPeers([]);
      return;
    }

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const host = window.location.host;
    const wsUrl = `${protocol}//${host}/api/v1/sync?docId=${encodeURIComponent(activeDocId)}&name=${encodeURIComponent(myPeerName)}`;

    let ws: WebSocket;
    try {
      ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        setIsCollabConnected(true);
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === "presence") {
            if (data.action === "join" && data.peerId !== myPeerId) {
              setConnectedPeers((prev) => {
                if (prev.some((p) => p.peerId === data.peerId)) return prev;
                const color = PEER_COLORS[Math.abs(data.peerId.split("").reduce((a: number, c: string) => a + c.charCodeAt(0), 0)) % PEER_COLORS.length] || "#6366f1";
                return [...prev, { peerId: data.peerId, name: data.name || "Peer", color }];
              });
            } else if (data.action === "leave") {
              setConnectedPeers((prev) => prev.filter((p) => p.peerId !== data.peerId));
            }
          } else if (data.type === "edit" && data.peerId !== myPeerId && data.docId === activeDocIdRef.current) {
            if (data.title !== undefined) setDocTitle(data.title);
            if (data.content !== undefined) setDocContent(data.content);
            if (data.icon !== undefined) setDocIcon(data.icon);
            if (data.coverUrl !== undefined) setDocCover(data.coverUrl);
          }
        } catch (e) {
          console.error("WS error parsing message", e);
        }
      };

      ws.onclose = () => {
        setIsCollabConnected(false);
      };
    } catch (e) {
      console.warn("Collab WS not available", e);
    }

    return () => {
      if (ws) ws.close();
      wsRef.current = null;
    };
  }, [activeDocId, myPeerId, myPeerName]);

  const loadRevisions = useCallback(async (docId: string) => {
    setLoadingRevisions(true);
    try {
      const res = await fetch(`/api/v1/documents/${docId}/revisions`);
      if (res.ok) {
        const data: DocumentRevision[] = await res.json();
        setRevisions(data);
        if (data.length > 0) {
          setSelectedRevision(data[0]);
        } else {
          setSelectedRevision(null);
        }
      }
    } catch (err) {
      console.error("Failed to load revisions", err);
    } finally {
      setLoadingRevisions(false);
    }
  }, []);

  const selectDocument = (doc: Document) => {
    setActiveDocId(doc.id);
    setDocTitle(doc.title);
    setDocContent(doc.content);
    setDocIcon(doc.icon || "📄");
    setDocFolder(doc.folder || "General");
    setDocCover(doc.coverUrl || "");
    setSaveStatus("saved");
    setShowCoverPicker(false);
    setShowEmojiPicker(false);
    setShowSlashMenu(false);
    if (showRevisions) {
      loadRevisions(doc.id);
    }
  };

  const createNewDocument = async () => {
    try {
      const res = await fetch("/api/v1/documents", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          title: "Untitled Document",
          content: "# Untitled Document\n\nStart typing your document or type '/' for blocks...",
          folder: selectedFolder || "General",
          icon: "📄",
          coverUrl: "",
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
    async (title: string, content: string, folder: string, icon: string, coverUrl: string) => {
      if (!activeDocId) return;
      setSaveStatus("saving");
      try {
        const res = await fetch(`/api/v1/documents/${activeDocId}`, {
          method: "PATCH",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ title, content, folder, icon, coverUrl }),
        });
        if (res.ok) {
          const updated: Document = await res.json();
          setDocuments((prev) => prev.map((d) => (d.id === updated.id ? updated : d)));
          setSaveStatus("saved");

          // Broadcast to connected peers via WebSocket
          if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
            wsRef.current.send(
              JSON.stringify({
                type: "edit",
                docId: activeDocId,
                peerId: myPeerId,
                title,
                content,
                icon,
                coverUrl,
              })
            );
          }
        } else {
          setSaveStatus("unsaved");
        }
      } catch (err) {
        setSaveStatus("unsaved");
      }
    },
    [activeDocId, myPeerId]
  );

  const onDocChange = (updates: {
    title?: string;
    content?: string;
    folder?: string;
    icon?: string;
    coverUrl?: string;
  }) => {
    const nextTitle = updates.title !== undefined ? updates.title : docTitle;
    const nextContent = updates.content !== undefined ? updates.content : docContent;
    const nextFolder = updates.folder !== undefined ? updates.folder : docFolder;
    const nextIcon = updates.icon !== undefined ? updates.icon : docIcon;
    const nextCover = updates.coverUrl !== undefined ? updates.coverUrl : docCover;

    if (updates.title !== undefined) setDocTitle(nextTitle);
    if (updates.content !== undefined) setDocContent(nextContent);
    if (updates.folder !== undefined) setDocFolder(nextFolder);
    if (updates.icon !== undefined) setDocIcon(nextIcon);
    if (updates.coverUrl !== undefined) setDocCover(nextCover);

    setSaveStatus("unsaved");

    if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    saveTimerRef.current = setTimeout(() => {
      saveDocument(nextTitle, nextContent, nextFolder, nextIcon, nextCover);
    }, 800);
  };

  const handleCreateSnapshot = async () => {
    if (!activeDocId) return;
    try {
      const res = await fetch(`/api/v1/documents/${activeDocId}/revisions`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ title: docTitle, content: docContent }),
      });
      if (res.ok) {
        loadRevisions(activeDocId);
        showToast("Snapshot created successfully!");
      }
    } catch (err) {
      console.error("Failed to create snapshot", err);
    }
  };

  const handleRestoreRevision = async (revId: string) => {
    if (!activeDocId) return;
    if (!confirm("Restore this revision? Your current state will be saved as a revision.")) return;
    try {
      const res = await fetch(`/api/v1/documents/${activeDocId}/revisions/${revId}/restore`, {
        method: "POST",
      });
      if (res.ok) {
        const restored: Document = await res.json();
        setDocTitle(restored.title);
        setDocContent(restored.content);
        setDocuments((prev) => prev.map((d) => (d.id === restored.id ? restored : d)));
        loadRevisions(activeDocId);
        showToast("Revision restored!");
      }
    } catch (err) {
      console.error("Failed to restore revision", err);
    }
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
        showToast("Document deleted");
      }
    } catch (err) {
      console.error("Failed to delete document", err);
    }
  };

  // Block & formatting inserters
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

  const insertTable = () => {
    const tableTemplate = "\n| Column 1 | Column 2 | Column 3 |\n| --- | --- | --- |\n| Data 1 | Data 2 | Data 3 |\n| Data 4 | Data 5 | Data 6 |\n\n";
    insertFormatting(tableTemplate, "");
  };

  const addTableRow = () => {
    const textarea = document.getElementById("doc-textarea") as HTMLTextAreaElement;
    if (!textarea) return;
    const text = textarea.value;
    const newRow = "| New Cell 1 | New Cell 2 | New Cell 3 |\n";
    onDocChange({ content: text + (text.endsWith("\n") ? "" : "\n") + newRow });
    showToast("Added table row");
  };

  const addTableCol = () => {
    const textarea = document.getElementById("doc-textarea") as HTMLTextAreaElement;
    if (!textarea) return;
    const text = textarea.value;
    const lines = text.split("\n");
    const updated = lines.map((line) => {
      if (line.trim().startsWith("|") && line.trim().endsWith("|")) {
        if (/^\|(?:\s*:?-+:?\s*\|)+$/.test(line.trim())) {
          return `${line.trim().slice(0, -1)} --- |`;
        }
        return `${line.trim().slice(0, -1)} Extra |`;
      }
      return line;
    });
    onDocChange({ content: updated.join("\n") });
    showToast("Added table column");
  };

  const insertCallout = (type: "tip" | "warning" | "danger" | "info") => {
    const title = type.charAt(0).toUpperCase() + type.slice(1);
    const template = `\n:::${type} ${title}\nAdd your callout note or warning description here.\n:::\n\n`;
    insertFormatting(template, "");
  };

  // Slash commands
  const handleTextareaKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === "/" && !showSlashMenu) {
      setShowSlashMenu(true);
      setSlashMenuFilter("");
    } else if (e.key === "Escape" && showSlashMenu) {
      setShowSlashMenu(false);
    }
  };

  const applySlashCommand = (cmd: string) => {
    const textarea = document.getElementById("doc-textarea") as HTMLTextAreaElement;
    if (!textarea) return;
    const cursor = textarea.selectionStart;
    const text = textarea.value;
    // Remove the trailing slash before cursor if present
    const sliceStart = cursor > 0 && text[cursor - 1] === "/" ? cursor - 1 : cursor;
    const before = text.substring(0, sliceStart);
    const after = text.substring(cursor);

    let insertion = "";
    switch (cmd) {
      case "h1": insertion = "# Heading 1\n\n"; break;
      case "h2": insertion = "## Heading 2\n\n"; break;
      case "h3": insertion = "### Heading 3\n\n"; break;
      case "bullet": insertion = "- List item\n"; break;
      case "number": insertion = "1. First item\n"; break;
      case "todo": insertion = "- [ ] Task item\n"; break;
      case "table": insertion = "| Header 1 | Header 2 | Header 3 |\n| --- | --- | --- |\n| Cell 1 | Cell 2 | Cell 3 |\n\n"; break;
      case "quote": insertion = "> Important quote or citation\n\n"; break;
      case "code": insertion = "```ts\n// Enter code snippet here\n```\n\n"; break;
      case "tip": insertion = ":::tip Quick Tip\nKey insight or recommended practice.\n:::\n\n"; break;
      case "warning": insertion = ":::warning Warning\nCritical alert or compatibility note.\n:::\n\n"; break;
      case "divider": insertion = "---\n\n"; break;
      default: break;
    }

    onDocChange({ content: before + insertion + after });
    setShowSlashMenu(false);
    setTimeout(() => {
      textarea.focus();
      textarea.setSelectionRange(before.length + insertion.length, before.length + insertion.length);
    }, 10);
  };

  // Export actions
  const exportMarkdown = () => {
    const blob = new Blob([`# ${docTitle}\n\n${docContent}`], { type: "text/markdown;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${(docTitle || "document").toLowerCase().replace(/\s+/g, "-")}.md`;
    a.click();
    URL.revokeObjectURL(url);
    setShowExportMenu(false);
    showToast("Downloaded Markdown file");
  };

  const exportHtml = () => {
    const fullHtml = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>${docTitle || "Document"}</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; max-width: 820px; margin: 40px auto; padding: 0 20px; color: #18181b; line-height: 1.6; }
    h1, h2, h3 { color: #09090b; }
    h1 { border-bottom: 2px solid #e4e4e7; padding-bottom: 8px; }
    table { width: 100%; border-collapse: collapse; margin: 20px 0; }
    th, td { border: 1px solid #d4d4d8; padding: 10px 14px; text-align: left; }
    th { background: #f4f4f5; font-weight: 600; }
    pre { background: #18181b; color: #a5b4fc; padding: 16px; border-radius: 8px; overflow-x: auto; }
    code { font-family: monospace; }
    blockquote { border-left: 4px solid #6366f1; margin: 16px 0; padding-left: 16px; color: #52525b; font-style: italic; }
    hr { border: none; border-top: 1px solid #e4e4e7; margin: 24px 0; }
  </style>
</head>
<body>
  <h1>${docTitle || "Untitled Document"}</h1>
  ${renderDocHtml(docContent)}
</body>
</html>`;
    const blob = new Blob([fullHtml], { type: "text/html;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${(docTitle || "document").toLowerCase().replace(/\s+/g, "-")}.html`;
    a.click();
    URL.revokeObjectURL(url);
    setShowExportMenu(false);
    showToast("Downloaded HTML file");
  };

  const copyHtmlToClipboard = async () => {
    try {
      const rendered = renderDocHtml(docContent);
      await navigator.clipboard.writeText(rendered);
      setShowExportMenu(false);
      showToast("Rendered HTML copied to clipboard!");
    } catch {
      showToast("Failed to copy HTML");
    }
  };

  const exportPlainText = () => {
    const plain = docContent.replace(/[#*`_>\[\]]/g, "");
    const blob = new Blob([`${docTitle}\n\n${plain}`], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${(docTitle || "document").toLowerCase().replace(/\s+/g, "-")}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    setShowExportMenu(false);
    showToast("Downloaded Plain Text file");
  };

  const printDocument = () => {
    setShowExportMenu(false);
    window.print();
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
      {/* Toast Notification */}
      {toastMessage && (
        <div className="fixed bottom-5 right-5 bg-indigo-600 text-white text-xs px-4 py-2.5 rounded-xl shadow-2xl z-50 flex items-center gap-2 animate-in fade-in slide-in-from-bottom-2">
          <CheckCircle2 className="w-4 h-4 text-white" />
          <span>{toastMessage}</span>
        </div>
      )}

      {/* ── Left Sidebar: Folders & Workspaces ── */}
      <aside className="w-64 flex-shrink-0 bg-nexus-900 border-r border-nexus-800 flex flex-col justify-between print:hidden">
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
      <section className="w-72 flex-shrink-0 bg-nexus-950 border-r border-nexus-800 flex flex-col print:hidden">
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
      <main className="flex-1 bg-nexus-950 flex flex-col overflow-hidden relative">
        {activeDoc ? (
          <>
            {/* Header Toolbar */}
            <header className="h-14 border-b border-nexus-800 px-6 flex items-center justify-between bg-nexus-950 print:hidden z-20">
              <div className="flex items-center gap-3">
                {/* Icon selector */}
                <div className="relative">
                  <button
                    onClick={() => {
                      setShowEmojiPicker(!showEmojiPicker);
                      setShowCoverPicker(false);
                    }}
                    className="w-8 h-8 rounded-lg bg-nexus-900 border border-nexus-800 flex items-center justify-center text-lg hover:bg-nexus-800 transition-colors"
                    title="Change icon"
                  >
                    {docIcon}
                  </button>
                  {showEmojiPicker && (
                    <div className="absolute top-10 left-0 bg-nexus-900 border border-nexus-800 rounded-xl p-3 shadow-2xl w-72 z-50 animate-in fade-in duration-150">
                      <div className="flex items-center justify-between mb-2 pb-2 border-b border-nexus-800">
                        <span className="text-xs font-semibold text-zinc-200">Select Icon</span>
                        <button
                          onClick={() => {
                            onDocChange({ icon: "📄" });
                            setShowEmojiPicker(false);
                          }}
                          className="text-[10px] text-zinc-400 hover:text-white"
                        >
                          Reset
                        </button>
                      </div>
                      <input
                        type="text"
                        placeholder="Search icons..."
                        value={emojiSearch}
                        onChange={(e) => setEmojiSearch(e.target.value)}
                        className="w-full bg-nexus-950 border border-nexus-800 rounded-lg px-2.5 py-1 text-xs text-zinc-200 placeholder-zinc-600 mb-2 focus:outline-none"
                      />
                      <div className="max-h-56 overflow-y-auto space-y-2">
                        {EMOJI_CATEGORIES.map((cat) => {
                          const filtered = cat.emojis.filter((e) => !emojiSearch || e.includes(emojiSearch));
                          if (filtered.length === 0) return null;
                          return (
                            <div key={cat.name}>
                              <div className="text-[10px] font-medium text-zinc-500 uppercase tracking-wider mb-1 px-1">
                                {cat.name}
                              </div>
                              <div className="grid grid-cols-7 gap-1">
                                {filtered.map((emoji) => (
                                  <button
                                    key={emoji}
                                    onClick={() => {
                                      onDocChange({ icon: emoji });
                                      setShowEmojiPicker(false);
                                    }}
                                    className="w-7 h-7 flex items-center justify-center rounded hover:bg-nexus-800 text-base transition-transform active:scale-90"
                                  >
                                    {emoji}
                                  </button>
                                ))}
                              </div>
                            </div>
                          );
                        })}
                      </div>
                    </div>
                  )}
                </div>

                {/* Cover Picker Trigger Button */}
                <div className="relative">
                  <button
                    onClick={() => {
                      setShowCoverPicker(!showCoverPicker);
                      setShowEmojiPicker(false);
                    }}
                    className={`flex items-center gap-1.5 px-2.5 py-1.5 rounded-lg border text-xs transition-colors ${
                      docCover
                        ? "bg-indigo-600/20 border-indigo-500/40 text-indigo-300"
                        : "bg-nexus-900 border-nexus-800 text-zinc-400 hover:text-white hover:bg-nexus-850"
                    }`}
                    title="Change document cover"
                  >
                    <Image className="w-3.5 h-3.5" />
                    <span>{docCover ? "Cover" : "Add Cover"}</span>
                  </button>

                  {showCoverPicker && (
                    <div className="absolute top-10 left-0 bg-nexus-900 border border-nexus-800 rounded-xl p-3 shadow-2xl w-80 z-50 animate-in fade-in duration-150">
                      <div className="flex items-center justify-between mb-2 pb-2 border-b border-nexus-800">
                        <span className="text-xs font-semibold text-zinc-200">Document Cover</span>
                        {docCover && (
                          <button
                            onClick={() => {
                              onDocChange({ coverUrl: "" });
                              setShowCoverPicker(false);
                            }}
                            className="text-[10px] text-red-400 hover:underline"
                          >
                            Remove
                          </button>
                        )}
                      </div>

                      <div className="text-[11px] font-medium text-zinc-400 mb-1.5">Preset Themes</div>
                      <div className="grid grid-cols-2 gap-2 mb-3">
                        {COVER_PRESETS.map((preset) => (
                          <button
                            key={preset.name}
                            onClick={() => {
                              onDocChange({ coverUrl: preset.style });
                              setShowCoverPicker(false);
                            }}
                            className="h-12 rounded-lg border border-nexus-700/60 p-2 flex items-end justify-start text-[10px] font-medium text-white shadow-sm hover:ring-2 hover:ring-indigo-500 transition-all text-left"
                            style={{ background: preset.style }}
                          >
                            <span className="bg-black/50 px-1.5 py-0.5 rounded backdrop-blur-sm">
                              {preset.name}
                            </span>
                          </button>
                        ))}
                      </div>

                      <div className="text-[11px] font-medium text-zinc-400 mb-1.5">Custom Image URL</div>
                      <div className="flex gap-1.5">
                        <input
                          type="text"
                          placeholder="https://images.unsplash.com/..."
                          value={customCoverUrl}
                          onChange={(e) => setCustomCoverUrl(e.target.value)}
                          className="flex-1 bg-nexus-950 border border-nexus-800 rounded-lg px-2.5 py-1 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none"
                        />
                        <button
                          onClick={() => {
                            if (customCoverUrl.trim()) {
                              onDocChange({ coverUrl: customCoverUrl.trim() });
                              setCustomCoverUrl("");
                              setShowCoverPicker(false);
                            }
                          }}
                          className="bg-indigo-600 hover:bg-indigo-500 text-white text-xs px-2.5 py-1 rounded-lg transition-colors"
                        >
                          Apply
                        </button>
                      </div>
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
                  onClick={() => insertFormatting("~~", "~~")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800 line-through text-xs px-1.5"
                  title="Strikethrough"
                >
                  S
                </button>
                <button
                  onClick={() => insertFormatting("- ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Bullet List"
                >
                  <List className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("1. ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Numbered List"
                >
                  <ListOrdered className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => insertFormatting("- [ ] ")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Task Checklist"
                >
                  <CheckSquare className="w-3.5 h-3.5" />
                </button>
                <div className="w-[1px] h-4 bg-nexus-800 mx-1" />
                {/* Callout Quick Select */}
                <button
                  onClick={() => insertCallout("tip")}
                  className="p-1 hover:text-emerald-400 rounded hover:bg-nexus-800"
                  title="Tip Callout"
                >
                  <Sparkles className="w-3.5 h-3.5 text-emerald-400" />
                </button>
                <button
                  onClick={() => insertCallout("warning")}
                  className="p-1 hover:text-amber-400 rounded hover:bg-nexus-800"
                  title="Warning Callout"
                >
                  <AlertTriangle className="w-3.5 h-3.5 text-amber-400" />
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
                <div className="w-[1px] h-4 bg-nexus-800 mx-1" />
                {/* Table & Structure */}
                <button
                  onClick={insertTable}
                  className="p-1 hover:text-indigo-400 rounded hover:bg-nexus-800 flex items-center gap-1"
                  title="Insert 3x3 Table"
                >
                  <Table className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={addTableRow}
                  className="px-1 py-0.5 text-[10px] hover:text-indigo-300 rounded hover:bg-nexus-800 font-mono"
                  title="Add Row to Table"
                >
                  +Row
                </button>
                <button
                  onClick={addTableCol}
                  className="px-1 py-0.5 text-[10px] hover:text-indigo-300 rounded hover:bg-nexus-800 font-mono"
                  title="Add Column to Table"
                >
                  +Col
                </button>
                <button
                  onClick={() => insertFormatting("\n---\n\n")}
                  className="p-1 hover:text-white rounded hover:bg-nexus-800"
                  title="Divider Rule"
                >
                  <Minus className="w-3.5 h-3.5" />
                </button>
              </div>

              {/* Status and Action Buttons */}
              <div className="flex items-center gap-2">
                {/* Collaborative Sync & Presence indicator */}
                <div className="flex items-center gap-1.5 mr-2">
                  <div
                    className={`w-2 h-2 rounded-full ${
                      isCollabConnected ? "bg-emerald-400 animate-pulse" : "bg-zinc-600"
                    }`}
                    title={isCollabConnected ? "Real-time sync active" : "Offline sync"}
                  />
                  {connectedPeers.length > 0 && (
                    <div className="flex items-center -space-x-1.5" title={`${connectedPeers.length + 1} users editing`}>
                      {connectedPeers.map((p) => (
                        <div
                          key={p.peerId}
                          className="w-5 h-5 rounded-full border border-nexus-900 flex items-center justify-center text-[9px] font-bold text-white shadow-sm"
                          style={{ backgroundColor: p.color }}
                        >
                          {p.name.charAt(0)}
                        </div>
                      ))}
                    </div>
                  )}
                </div>

                <span className="text-[11px] text-zinc-500 flex items-center gap-1 font-mono mr-2">
                  {saveStatus === "saving" && "Saving..."}
                  {saveStatus === "saved" && (
                    <>
                      <Check className="w-3 h-3 text-indigo-400" />
                      <span>Saved</span>
                    </>
                  )}
                  {saveStatus === "unsaved" && "Unsaved"}
                </span>

                {/* History Drawer Toggle */}
                <button
                  onClick={() => {
                    const next = !showRevisions;
                    setShowRevisions(next);
                    if (next && activeDocId) loadRevisions(activeDocId);
                  }}
                  className={`flex items-center gap-1.5 px-2.5 py-1.5 rounded-lg border text-xs transition-colors ${
                    showRevisions
                      ? "bg-indigo-600 text-white border-indigo-500"
                      : "bg-nexus-900 border-nexus-800 text-zinc-300 hover:text-white hover:bg-nexus-850"
                  }`}
                  title="Document Version History"
                >
                  <History className="w-3.5 h-3.5" />
                  <span className="hidden sm:inline">History</span>
                </button>

                {/* Export Dropdown */}
                <div className="relative">
                  <button
                    onClick={() => setShowExportMenu(!showExportMenu)}
                    className="flex items-center gap-1.5 px-2.5 py-1.5 rounded-lg border border-nexus-800 bg-nexus-900 text-zinc-300 hover:text-white hover:bg-nexus-850 text-xs transition-colors"
                  >
                    <Download className="w-3.5 h-3.5" />
                    <span className="hidden sm:inline">Export</span>
                    <ChevronDown className="w-3 h-3 text-zinc-500" />
                  </button>

                  {showExportMenu && (
                    <div className="absolute right-0 top-9 w-48 bg-nexus-900 border border-nexus-800 rounded-xl p-1.5 shadow-2xl z-50 flex flex-col gap-0.5">
                      <button
                        onClick={exportMarkdown}
                        className="flex items-center gap-2 px-3 py-2 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                      >
                        <FileDown className="w-3.5 h-3.5 text-indigo-400" />
                        <span>Markdown (.md)</span>
                      </button>
                      <button
                        onClick={exportHtml}
                        className="flex items-center gap-2 px-3 py-2 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                      >
                        <FileText className="w-3.5 h-3.5 text-emerald-400" />
                        <span>HTML Document</span>
                      </button>
                      <button
                        onClick={copyHtmlToClipboard}
                        className="flex items-center gap-2 px-3 py-2 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                      >
                        <Copy className="w-3.5 h-3.5 text-cyan-400" />
                        <span>Copy Clean HTML</span>
                      </button>
                      <button
                        onClick={exportPlainText}
                        className="flex items-center gap-2 px-3 py-2 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                      >
                        <Layers className="w-3.5 h-3.5 text-purple-400" />
                        <span>Plain Text (.txt)</span>
                      </button>
                      <button
                        onClick={printDocument}
                        className="flex items-center gap-2 px-3 py-2 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                      >
                        <Printer className="w-3.5 h-3.5 text-amber-400" />
                        <span>Print / Save as PDF</span>
                      </button>
                    </div>
                  )}
                </div>
              </div>
            </header>

            {/* Document Cover Banner */}
            {docCover && (
              <div
                className="w-full h-32 flex-shrink-0 relative group transition-all"
                style={{
                  background: docCover.startsWith("http")
                    ? `url("${docCover}") center/cover no-repeat`
                    : docCover,
                }}
              >
                <div className="absolute inset-0 bg-black/20 group-hover:bg-black/40 transition-colors" />
                <div className="absolute right-4 bottom-3 opacity-0 group-hover:opacity-100 transition-opacity flex items-center gap-2">
                  <button
                    onClick={() => setShowCoverPicker(true)}
                    className="bg-black/60 hover:bg-black/80 backdrop-blur-md text-white text-xs px-2.5 py-1 rounded-md flex items-center gap-1.5 transition-colors border border-white/10"
                  >
                    <Palette className="w-3 h-3" />
                    <span>Change Cover</span>
                  </button>
                  <button
                    onClick={() => onDocChange({ coverUrl: "" })}
                    className="bg-black/60 hover:bg-red-950/80 backdrop-blur-md text-red-300 text-xs px-2.5 py-1 rounded-md flex items-center gap-1 transition-colors border border-white/10"
                  >
                    <X className="w-3 h-3" />
                    <span>Remove</span>
                  </button>
                </div>
              </div>
            )}

            {/* Document Body: Split Editor & Preview Canvas */}
            <div className="flex-1 flex overflow-hidden relative">
              {/* Left Edit Pane */}
              <div className="flex-1 flex flex-col border-r border-nexus-800/80 p-8 overflow-y-auto print:hidden relative">
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
                  onKeyDown={handleTextareaKeyDown}
                  placeholder="Type your markdown document or type '/' for blocks..."
                  className="w-full flex-1 bg-transparent resize-none font-mono text-sm leading-relaxed text-zinc-200 placeholder-zinc-700 focus:outline-none"
                />

                {/* Slash Command Palette */}
                {showSlashMenu && (
                  <div className="absolute top-24 left-8 w-64 bg-nexus-900 border border-nexus-800 rounded-xl shadow-2xl p-1.5 z-50 flex flex-col gap-0.5 animate-in fade-in duration-100">
                    <div className="px-2 py-1 text-[10px] font-semibold text-zinc-500 uppercase tracking-wider flex justify-between items-center">
                      <span>Insert Block</span>
                      <span className="font-mono text-zinc-600">Esc to close</span>
                    </div>
                    <button
                      onClick={() => applySlashCommand("h1")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Heading1 className="w-3.5 h-3.5 text-indigo-400" />
                      <span>Heading 1</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("h2")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Heading2 className="w-3.5 h-3.5 text-indigo-400" />
                      <span>Heading 2</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("h3")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Heading3 className="w-3.5 h-3.5 text-indigo-400" />
                      <span>Heading 3</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("bullet")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <List className="w-3.5 h-3.5 text-zinc-400" />
                      <span>Bullet List</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("todo")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <CheckSquare className="w-3.5 h-3.5 text-zinc-400" />
                      <span>Todo Checklist</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("table")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Table className="w-3.5 h-3.5 text-indigo-400" />
                      <span>Data Table (3x3)</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("tip")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Sparkles className="w-3.5 h-3.5 text-emerald-400" />
                      <span>Tip Callout</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("warning")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <AlertTriangle className="w-3.5 h-3.5 text-amber-400" />
                      <span>Warning Callout</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("code")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Code className="w-3.5 h-3.5 text-indigo-400" />
                      <span>Code Block</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("quote")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Quote className="w-3.5 h-3.5 text-zinc-400" />
                      <span>Blockquote</span>
                    </button>
                    <button
                      onClick={() => applySlashCommand("divider")}
                      className="flex items-center gap-2 px-2.5 py-1.5 text-xs text-zinc-300 hover:text-white hover:bg-nexus-800 rounded-lg text-left transition-colors"
                    >
                      <Minus className="w-3.5 h-3.5 text-zinc-400" />
                      <span>Divider</span>
                    </button>
                  </div>
                )}
              </div>

              {/* Right Live Rendered Pane */}
              <div className="flex-1 p-8 overflow-y-auto bg-nexus-950/40 print:p-0 print:bg-white print:text-black">
                <div className="max-w-2xl mx-auto">
                  <div className="flex items-center gap-2 mb-2">
                    <span className="text-2xl">{docIcon}</span>
                    <span className="text-xs uppercase font-semibold text-zinc-500 tracking-wider">
                      {docFolder}
                    </span>
                  </div>
                  <h1 className="text-3xl font-extrabold text-white print:text-black mb-6 pb-2 border-b border-nexus-800 print:border-black">
                    {docTitle || "Untitled Document"}
                  </h1>
                  <div
                    className="prose prose-invert print:prose max-w-none text-sm"
                    dangerouslySetInnerHTML={{
                      __html: renderDocHtml(docContent),
                    }}
                  />
                </div>
              </div>
            </div>

            {/* Bottom Status / Metrics Bar */}
            <footer className="h-8 border-t border-nexus-800 px-6 flex items-center justify-between bg-nexus-900/60 text-[11px] text-zinc-500 font-mono print:hidden">
              <div className="flex items-center gap-4">
                <span>{metrics.words} words</span>
                <span>•</span>
                <span>{metrics.chars} characters</span>
                <span>•</span>
                <span>~{metrics.readTimeMinutes} min read</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="flex items-center gap-1.5">
                  <span className={`w-1.5 h-1.5 rounded-full ${isCollabConnected ? "bg-emerald-400" : "bg-zinc-600"}`} />
                  <span>{isCollabConnected ? "Live Peer Collab" : "Local Sync"}</span>
                </span>
                <span>•</span>
                <span>Markdown, Tables & Callouts</span>
              </div>
            </footer>

            {/* Slide-out Revision History Drawer */}
            {showRevisions && (
              <aside className="absolute top-14 bottom-0 right-0 w-80 bg-nexus-900 border-l border-nexus-800 shadow-2xl flex flex-col z-40 print:hidden animate-in slide-in-from-right duration-200">
                <div className="p-4 border-b border-nexus-800 flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <History className="w-4 h-4 text-indigo-400" />
                    <h3 className="text-xs font-semibold text-zinc-200 uppercase tracking-wider">
                      Revision History
                    </h3>
                  </div>
                  <div className="flex items-center gap-1">
                    <button
                      onClick={handleCreateSnapshot}
                      className="px-2 py-1 rounded bg-indigo-600/30 text-indigo-300 hover:bg-indigo-600/50 text-[11px] flex items-center gap-1"
                      title="Save manual checkpoint"
                    >
                      <Camera className="w-3 h-3" />
                      <span>Snapshot</span>
                    </button>
                    <button
                      onClick={() => setShowRevisions(false)}
                      className="text-zinc-500 hover:text-zinc-300 p-1 rounded"
                    >
                      <X className="w-4 h-4" />
                    </button>
                  </div>
                </div>

                <div className="flex-1 overflow-y-auto divide-y divide-nexus-800">
                  {loadingRevisions ? (
                    <div className="p-6 text-center text-xs text-zinc-500">
                      Loading revisions...
                    </div>
                  ) : revisions.length === 0 ? (
                    <div className="p-6 text-center text-xs text-zinc-600">
                      No past revisions found.
                    </div>
                  ) : (
                    revisions.map((rev, index) => {
                      const isSelected = selectedRevision?.id === rev.id;
                      const date = new Date(rev.createdAt);
                      const timeStr = date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                      const dateStr = date.toLocaleDateString([], { month: "short", day: "numeric" });
                      return (
                        <div
                          key={rev.id}
                          onClick={() => setSelectedRevision(rev)}
                          className={`p-3.5 cursor-pointer transition-colors ${
                            isSelected ? "bg-nexus-800/80 border-l-2 border-indigo-500" : "hover:bg-nexus-850"
                          }`}
                        >
                          <div className="flex items-center justify-between mb-1">
                            <span className="text-xs font-semibold text-zinc-200">
                              {rev.title || "Untitled Snapshot"}
                            </span>
                            {index === 0 && (
                              <span className="text-[9px] bg-indigo-500/20 text-indigo-300 px-1.5 py-0.5 rounded font-mono">
                                Latest
                              </span>
                            )}
                          </div>
                          <div className="flex items-center gap-1.5 text-[10px] text-zinc-500 font-mono mb-2">
                            <Clock className="w-3 h-3" />
                            <span>{dateStr} at {timeStr}</span>
                          </div>
                          <p className="text-[11px] text-zinc-400 line-clamp-2 leading-relaxed">
                            {rev.content.slice(0, 100) || "No content"}
                          </p>
                          <div className="mt-2.5 flex items-center justify-end">
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                handleRestoreRevision(rev.id);
                              }}
                              className="flex items-center gap-1 px-2 py-1 rounded bg-nexus-800 hover:bg-indigo-600 text-zinc-300 hover:text-white text-[11px] font-medium transition-colors border border-nexus-700"
                            >
                              <RotateCcw className="w-3 h-3" />
                              <span>Restore version</span>
                            </button>
                          </div>
                        </div>
                      );
                    })
                  )}
                </div>
              </aside>
            )}
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
