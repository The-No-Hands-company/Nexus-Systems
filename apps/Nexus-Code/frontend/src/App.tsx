import React, { useState, useEffect, useCallback, useMemo, useRef } from "react";
import {
  Code2,
  GitBranch,
  GitCommit,
  Folder,
  FileCode,
  Plus,
  Copy,
  Check,
  Search,
  Terminal,
  ShieldCheck,
  ChevronRight,
  Clock,
  Layers,
  Sparkles,
  ExternalLink,
} from "lucide-react";

interface Repository {
  id: string;
  name: string;
  description: string;
  defaultBranch: string;
  visibility: "public" | "private";
  createdAt: string;
  updatedAt: string;
}

interface RepoFile {
  id: string;
  repoId: string;
  branch: string;
  path: string;
  type: "file" | "dir";
  content: string;
  size: number;
  updatedAt: string;
}

interface CommitRecord {
  id: string;
  repoId: string;
  hash: string;
  branch: string;
  message: string;
  author: string;
  createdAt: string;
}

interface Snippet {
  id: string;
  title: string;
  language: string;
  code: string;
  tags: string;
  createdAt: string;
  updatedAt: string;
}

export default function App() {
  const [repos, setRepos] = useState<Repository[]>([]);
  const [selectedRepoId, setSelectedRepoId] = useState<string | null>(null);
  const [currentBranch, setCurrentBranch] = useState("main");
  const [activeTab, setActiveTab] = useState<"code" | "commits" | "snippets">("code");

  // Code Tab state
  const [files, setFiles] = useState<RepoFile[]>([]);
  const [selectedFilePath, setSelectedFilePath] = useState<string | null>(null);
  const [activeFileContent, setActiveFileContent] = useState<string>("");
  const [copiedCode, setCopiedCode] = useState(false);

  // Commits Tab state
  const [commits, setCommits] = useState<CommitRecord[]>([]);

  // Snippets Tab state
  const [snippets, setSnippets] = useState<Snippet[]>([]);
  const [selectedSnippetId, setSelectedSnippetId] = useState<string | null>(null);

  // Modals / Creators
  const [isAddingRepo, setIsAddingRepo] = useState(false);
  const [newRepoName, setNewRepoName] = useState("");
  const [newRepoDesc, setNewRepoDesc] = useState("");

  const [isAddingFile, setIsAddingFile] = useState(false);
  const [newFilePath, setNewFilePath] = useState("");
  const [newFileContent, setNewFileContent] = useState("");

  const selectedRepo = useMemo(() => {
    return repos.find((r) => r.id === selectedRepoId) || null;
  }, [repos, selectedRepoId]);

  const loadRepos = useCallback(async () => {
    try {
      const res = await fetch("/api/v1/repos");
      if (res.ok) {
        const data: Repository[] = await res.json();
        setRepos(data);
        if (data.length > 0 && !selectedRepoId) {
          setSelectedRepoId(data[0].id);
        }
      }
    } catch (err) {
      console.error("Failed to load repos", err);
    }
  }, [selectedRepoId]);

  const loadFiles = useCallback(async (repoId: string, branch = "main") => {
    try {
      const res = await fetch(`/api/v1/repos/${repoId}/files?branch=${encodeURIComponent(branch)}`);
      if (res.ok) {
        const data: RepoFile[] = await res.json();
        setFiles(data);
        if (data.length > 0) {
          const readme = data.find((f) => f.path.toLowerCase().includes("readme")) || data[0];
          setSelectedFilePath(readme.path);
          setActiveFileContent(readme.content);
        } else {
          setSelectedFilePath(null);
          setActiveFileContent("");
        }
      }
    } catch (err) {
      console.error("Failed to load files", err);
    }
  }, []);

  const loadCommits = useCallback(async (repoId: string, branch = "main") => {
    try {
      const res = await fetch(`/api/v1/repos/${repoId}/commits?branch=${encodeURIComponent(branch)}`);
      if (res.ok) {
        const data = await res.json();
        setCommits(data);
      }
    } catch (err) {
      console.error("Failed to load commits", err);
    }
  }, []);

  const loadSnippets = useCallback(async () => {
    try {
      const res = await fetch("/api/v1/snippets");
      if (res.ok) {
        const data: Snippet[] = await res.json();
        setSnippets(data);
        if (data.length > 0 && !selectedSnippetId) {
          setSelectedSnippetId(data[0].id);
        }
      }
    } catch (err) {
      console.error("Failed to load snippets", err);
    }
  }, [selectedSnippetId]);

  useEffect(() => {
    loadRepos();
  }, [loadRepos]);

  useEffect(() => {
    if (selectedRepoId) {
      loadFiles(selectedRepoId, currentBranch);
      loadCommits(selectedRepoId, currentBranch);
    }
  }, [selectedRepoId, currentBranch, loadFiles, loadCommits]);

  useEffect(() => {
    if (activeTab === "snippets") {
      loadSnippets();
    }
  }, [activeTab, loadSnippets]);

  const handleSelectFile = (file: RepoFile) => {
    setSelectedFilePath(file.path);
    setActiveFileContent(file.content);
  };

  const handleCopyCode = (text: string) => {
    navigator.clipboard.writeText(text);
    setCopiedCode(true);
    setTimeout(() => setCopiedCode(false), 2000);
  };

  const handleCreateRepo = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newRepoName.trim()) return;
    try {
      const res = await fetch("/api/v1/repos", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: newRepoName.trim(), description: newRepoDesc.trim() }),
      });
      if (res.ok) {
        const created: Repository = await res.json();
        setRepos((prev) => [created, ...prev]);
        setSelectedRepoId(created.id);
        setIsAddingRepo(false);
        setNewRepoName("");
        setNewRepoDesc("");
      }
    } catch (err) {
      console.error("Failed to create repo", err);
    }
  };

  const handleCreateFile = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!selectedRepoId || !newFilePath.trim()) return;
    try {
      const res = await fetch(`/api/v1/repos/${selectedRepoId}/files`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          path: newFilePath.trim(),
          content: newFileContent,
          message: `Create ${newFilePath.trim()}`,
        }),
      });
      if (res.ok) {
        setIsAddingFile(false);
        setNewFilePath("");
        setNewFileContent("");
        loadFiles(selectedRepoId, currentBranch);
        loadCommits(selectedRepoId, currentBranch);
      }
    } catch (err) {
      console.error("Failed to create file", err);
    }
  };

  return (
    <div className="flex h-screen w-screen bg-nexus-950 font-sans text-zinc-100 antialiased overflow-hidden select-none">
      {/* ── Left Sidebar: Repositories & Navigation ── */}
      <aside className="w-64 flex-shrink-0 bg-nexus-900 border-r border-nexus-800 flex flex-col justify-between">
        <div className="p-4 flex flex-col gap-4">
          <div className="flex items-center gap-2.5 px-2">
            <div className="w-8 h-8 rounded-lg bg-cyan-500/20 border border-cyan-500/30 flex items-center justify-center text-cyan-400">
              <Code2 className="w-4 h-4" />
            </div>
            <div>
              <h1 className="text-sm font-semibold tracking-wide text-zinc-100">Nexus Code</h1>
              <p className="text-[11px] text-zinc-500">Sovereign Git & Source</p>
            </div>
          </div>

          {/* Navigation Tabs */}
          <div className="flex flex-col gap-1 text-xs">
            <button
              onClick={() => setActiveTab("code")}
              className={`flex items-center gap-2.5 px-3 py-2 rounded-lg transition-colors ${
                activeTab === "code"
                  ? "bg-cyan-500/15 text-cyan-300 font-medium border border-cyan-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <FileCode className="w-3.5 h-3.5" />
              <span>Repositories & Code</span>
            </button>

            <button
              onClick={() => setActiveTab("commits")}
              className={`flex items-center gap-2.5 px-3 py-2 rounded-lg transition-colors ${
                activeTab === "commits"
                  ? "bg-cyan-500/15 text-cyan-300 font-medium border border-cyan-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <GitCommit className="w-3.5 h-3.5" />
              <span>Commit Tree ({commits.length})</span>
            </button>

            <button
              onClick={() => setActiveTab("snippets")}
              className={`flex items-center gap-2.5 px-3 py-2 rounded-lg transition-colors ${
                activeTab === "snippets"
                  ? "bg-cyan-500/15 text-cyan-300 font-medium border border-cyan-500/30"
                  : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
              }`}
            >
              <Layers className="w-3.5 h-3.5" />
              <span>Gists & Snippets</span>
            </button>
          </div>

          {/* Repositories List */}
          <div className="pt-2">
            <div className="flex items-center justify-between px-2 mb-2">
              <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-500">
                Repositories
              </span>
              <button
                onClick={() => setIsAddingRepo(true)}
                className="text-zinc-400 hover:text-cyan-400 transition-colors p-1 rounded hover:bg-nexus-800"
                title="New Repository"
              >
                <Plus className="w-3.5 h-3.5" />
              </button>
            </div>

            {isAddingRepo && (
              <form onSubmit={handleCreateRepo} className="mb-2 px-1 flex flex-col gap-1.5">
                <input
                  type="text"
                  autoFocus
                  placeholder="Repository name..."
                  value={newRepoName}
                  onChange={(e) => setNewRepoName(e.target.value)}
                  className="w-full bg-nexus-950 border border-cyan-500/50 rounded-lg px-2.5 py-1.5 text-xs text-zinc-200 placeholder-zinc-600 focus:outline-none"
                />
                <input
                  type="text"
                  placeholder="Description..."
                  value={newRepoDesc}
                  onChange={(e) => setNewRepoDesc(e.target.value)}
                  className="w-full bg-nexus-950 border border-nexus-800 rounded-lg px-2.5 py-1 text-xs text-zinc-300 placeholder-zinc-600 focus:outline-none"
                />
                <button
                  type="submit"
                  className="bg-cyan-600 hover:bg-cyan-500 text-white text-xs py-1 rounded font-medium"
                >
                  Create
                </button>
              </form>
            )}

            <div className="flex flex-col gap-0.5 max-h-56 overflow-y-auto">
              {repos.map((repo) => (
                <button
                  key={repo.id}
                  onClick={() => setSelectedRepoId(repo.id)}
                  className={`flex items-center justify-between px-3 py-1.5 rounded-lg text-xs transition-colors ${
                    selectedRepoId === repo.id
                      ? "bg-nexus-800 text-cyan-300 font-medium"
                      : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-850"
                  }`}
                >
                  <div className="flex items-center gap-2 truncate">
                    <Folder className="w-3 h-3 text-cyan-500 flex-shrink-0" />
                    <span className="truncate">{repo.name}</span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        </div>

        {/* Footer Identity */}
        <div className="p-3 border-t border-nexus-800/80 bg-nexus-950/40 text-[11px] text-zinc-500 flex items-center justify-between">
          <div className="flex items-center gap-1.5">
            <ShieldCheck className="w-3.5 h-3.5 text-cyan-400" />
            <span>Git Sovereign</span>
          </div>
          <span className="font-mono text-[10px] text-zinc-600">v1.0</span>
        </div>
      </aside>

      {/* ── Main View Pane ── */}
      <main className="flex-1 flex flex-col overflow-hidden bg-nexus-950">
        {selectedRepo ? (
          <>
            {/* Repository Context Bar */}
            <header className="h-14 border-b border-nexus-800 px-6 flex items-center justify-between bg-nexus-950">
              <div className="flex items-center gap-3">
                <span className="text-base font-bold text-white flex items-center gap-2">
                  <Folder className="w-4 h-4 text-cyan-400" />
                  {selectedRepo.name}
                </span>
                <span className="text-xs bg-nexus-900 border border-nexus-800 text-zinc-400 px-2 py-0.5 rounded-full">
                  {selectedRepo.visibility}
                </span>
                <div className="flex items-center gap-1.5 text-xs text-zinc-400 bg-nexus-900 border border-nexus-800 px-2 py-1 rounded-md">
                  <GitBranch className="w-3 h-3 text-cyan-400" />
                  <span>{currentBranch}</span>
                </div>
              </div>

              <div className="flex items-center gap-3">
                <button
                  onClick={() => setIsAddingFile(true)}
                  className="flex items-center gap-1 bg-cyan-600 hover:bg-cyan-500 text-white font-medium text-xs px-2.5 py-1.5 rounded-md transition-colors shadow-sm"
                >
                  <Plus className="w-3.5 h-3.5" />
                  <span>Add File</span>
                </button>
                <button
                  onClick={() => handleCopyCode(`git clone https://code.tnhc.dev/${selectedRepo.name}.git`)}
                  className="flex items-center gap-1 bg-nexus-900 hover:bg-nexus-850 border border-nexus-800 text-zinc-300 text-xs px-2.5 py-1.5 rounded-md transition-colors"
                  title="Copy clone command"
                >
                  <Copy className="w-3 h-3 text-zinc-400" />
                  <span>Clone</span>
                </button>
              </div>
            </header>

            {/* Modal: New File */}
            {isAddingFile && (
              <div className="p-4 bg-nexus-900 border-b border-nexus-800 flex flex-col gap-3">
                <div className="flex items-center justify-between">
                  <h3 className="text-xs font-semibold uppercase tracking-wider text-cyan-400">
                    Add New File to {selectedRepo.name}
                  </h3>
                  <button
                    onClick={() => setIsAddingFile(false)}
                    className="text-xs text-zinc-500 hover:text-zinc-300"
                  >
                    Cancel
                  </button>
                </div>
                <input
                  type="text"
                  placeholder="File path (e.g. src/utils.ts)..."
                  value={newFilePath}
                  onChange={(e) => setNewFilePath(e.target.value)}
                  className="bg-nexus-950 border border-nexus-800 rounded-lg px-3 py-1.5 text-xs text-zinc-100 placeholder-zinc-600 focus:outline-none"
                />
                <textarea
                  placeholder="File content..."
                  value={newFileContent}
                  onChange={(e) => setNewFileContent(e.target.value)}
                  rows={4}
                  className="bg-nexus-950 border border-nexus-800 rounded-lg p-3 text-xs font-mono text-zinc-200 placeholder-zinc-600 focus:outline-none"
                />
                <button
                  onClick={handleCreateFile}
                  className="self-end bg-cyan-600 hover:bg-cyan-500 text-white text-xs px-3 py-1.5 rounded font-medium"
                >
                  Commit & Save File
                </button>
              </div>
            )}

            {/* TAB: CODE */}
            {activeTab === "code" && (
              <div className="flex-1 flex overflow-hidden">
                {/* File Tree Left Pane */}
                <div className="w-64 border-r border-nexus-800 flex flex-col bg-nexus-950">
                  <div className="p-3 border-b border-nexus-800 text-xs font-semibold uppercase tracking-wider text-zinc-500">
                    Files ({files.length})
                  </div>
                  <div className="flex-1 overflow-y-auto divide-y divide-nexus-850">
                    {files.map((file) => {
                      const isSelected = file.path === selectedFilePath;
                      return (
                        <div
                          key={file.id}
                          onClick={() => handleSelectFile(file)}
                          className={`px-3 py-2 text-xs flex items-center justify-between cursor-pointer transition-colors ${
                            isSelected
                              ? "bg-nexus-900 border-l-2 border-cyan-500 text-cyan-300 font-medium"
                              : "text-zinc-400 hover:text-zinc-200 hover:bg-nexus-900/40"
                          }`}
                        >
                          <div className="flex items-center gap-2 truncate">
                            <FileCode className="w-3.5 h-3.5 text-zinc-500" />
                            <span className="truncate">{file.path}</span>
                          </div>
                          <span className="text-[10px] text-zinc-600">{file.size} B</span>
                        </div>
                      );
                    })}
                  </div>
                </div>

                {/* Source Code Viewer Right Pane */}
                <div className="flex-1 flex flex-col overflow-hidden bg-nexus-950/70">
                  {selectedFilePath ? (
                    <>
                      <div className="h-10 border-b border-nexus-800 px-6 flex items-center justify-between bg-nexus-900/40 text-xs">
                        <span className="font-mono text-zinc-300">{selectedFilePath}</span>
                        <button
                          onClick={() => handleCopyCode(activeFileContent)}
                          className="flex items-center gap-1 text-zinc-400 hover:text-cyan-400 transition-colors"
                        >
                          {copiedCode ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
                          <span>{copiedCode ? "Copied" : "Copy Code"}</span>
                        </button>
                      </div>

                      <div className="flex-1 overflow-auto p-6 font-mono text-xs leading-relaxed">
                        <table className="w-full border-collapse">
                          <tbody>
                            {activeFileContent.split("\n").map((line, idx) => (
                              <tr key={idx} className="hover:bg-nexus-900/40">
                                <td className="w-12 select-none text-right pr-4 text-zinc-600 align-top">
                                  {idx + 1}
                                </td>
                                <td className="text-zinc-200 whitespace-pre font-mono">{line}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </>
                  ) : (
                    <div className="flex-1 flex flex-col items-center justify-center text-zinc-600 text-xs">
                      No file selected
                    </div>
                  )}
                </div>
              </div>
            )}

            {/* TAB: COMMITS */}
            {activeTab === "commits" && (
              <div className="flex-1 p-6 overflow-y-auto">
                <div className="max-w-3xl mx-auto space-y-3">
                  <h2 className="text-xs font-semibold uppercase tracking-wider text-zinc-400 mb-4">
                    Commit History ({commits.length})
                  </h2>
                  {commits.map((c) => (
                    <div
                      key={c.id}
                      className="p-3 bg-nexus-900 border border-nexus-800 rounded-lg flex items-center justify-between"
                    >
                      <div className="flex items-center gap-3">
                        <GitCommit className="w-4 h-4 text-cyan-400 flex-shrink-0" />
                        <div>
                          <div className="text-xs font-medium text-zinc-200">{c.message}</div>
                          <div className="text-[11px] text-zinc-500">
                            {c.author} committed on {new Date(c.createdAt).toLocaleString()}
                          </div>
                        </div>
                      </div>
                      <span className="font-mono text-xs bg-nexus-950 border border-nexus-800 px-2 py-0.5 rounded text-cyan-300">
                        {c.hash.substring(0, 7)}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* TAB: SNIPPETS */}
            {activeTab === "snippets" && (
              <div className="flex-1 p-6 overflow-y-auto">
                <div className="max-w-3xl mx-auto space-y-4">
                  <h2 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
                    Sovereign Gists & Snippets
                  </h2>
                  {snippets.length === 0 ? (
                    <p className="text-xs text-zinc-600">No snippets created yet.</p>
                  ) : (
                    snippets.map((snip) => (
                      <div
                        key={snip.id}
                        className="bg-nexus-900 border border-nexus-800 rounded-xl overflow-hidden"
                      >
                        <div className="px-4 py-2 border-b border-nexus-800 flex items-center justify-between bg-nexus-950/40">
                          <span className="text-xs font-semibold text-zinc-200">{snip.title}</span>
                          <span className="text-[10px] bg-nexus-800 px-1.5 py-0.5 rounded text-cyan-300 font-mono">
                            {snip.language}
                          </span>
                        </div>
                        <pre className="p-4 text-xs font-mono text-zinc-300 bg-nexus-950 overflow-x-auto">
                          <code>{snip.code}</code>
                        </pre>
                      </div>
                    ))
                  )}
                </div>
              </div>
            )}
          </>
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center text-zinc-600 gap-3">
            <Code2 className="w-10 h-10 text-zinc-700" />
            <p className="text-sm">Select or create a repository to explore</p>
          </div>
        )}
      </main>
    </div>
  );
}
