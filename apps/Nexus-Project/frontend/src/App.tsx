import { useEffect, useState } from "react";
import type { Project } from "./types";
import { api, ApiContext } from "./runtime";
import { HomeView } from "./views/Home";
import { BoardView } from "./views/Board";
import { ListView } from "./views/List";
import { TimelineView } from "./views/Timeline";

type View = "home" | "board" | "list" | "timeline";

const NAV_ITEMS: { id: View; label: string }[] = [
  { id: "board", label: "Board" },
  { id: "list", label: "List" },
  { id: "timeline", label: "Timeline" },
];

export default function App() {
  const [view, setView] = useState<View>("home");
  const [project, setProject] = useState<Project | null>(null);

  const selectProject = (p: Project) => { setProject(p); setView("board"); };
  const goHome = () => { setProject(null); setView("home"); };

  useEffect(() => {
    if (!project) {
      document.title = "Project";
    } else {
      const label = NAV_ITEMS.find(n => n.id === view)?.label ?? "Project";
      document.title = `${project.name} — ${label}`;
    }
  }, [view, project]);

  return (
    <ApiContext.Provider value={api}>
      <div className="flex flex-col h-screen bg-zinc-900 text-zinc-200">
        <a
          href="#main-content"
          className="sr-only focus:not-sr-only focus:absolute focus:z-50 focus:top-2 focus:left-2 focus:rounded focus:bg-zinc-800 focus:px-4 focus:py-2 focus:text-zinc-100 focus:ring-2 focus:ring-accent focus:outline-none"
        >
          Skip to main content
        </a>
        <header role="banner" className="flex flex-wrap items-center gap-2 px-4 py-2 min-h-12 border-b border-zinc-700 flex-shrink-0">
          <button
            type="button"
            onClick={goHome}
            aria-label="Nexus Project — home"
            className="flex items-center gap-1.5 font-semibold text-sm text-zinc-100 hover:opacity-90 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent rounded transition-opacity"
          >
            <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true" className="text-accent shrink-0">
              <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z" />
            </svg>
            <span>Project</span>
          </button>

          {project && (
            <>
              <span className="text-zinc-700" aria-hidden="true">/</span>
              <span className="text-sm text-zinc-400 truncate max-w-[10rem] sm:max-w-[16rem]">{project.name}</span>
              <nav className="flex gap-0.5 ml-1" aria-label="Project views">
                {NAV_ITEMS.map(item => (
                  <button
                    key={item.id}
                    type="button"
                    onClick={() => setView(item.id)}
                    aria-current={view === item.id ? "page" : undefined}
                    className={`px-3 py-1.5 rounded text-sm transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
                      view === item.id
                        ? "bg-zinc-800 text-zinc-100"
                        : "text-zinc-500 hover:text-zinc-300"
                    }`}
                  >
                    {item.label}
                  </button>
                ))}
              </nav>
            </>
          )}
        </header>

        <main id="main-content" className="flex-1 overflow-hidden">
          {view === "home" && <HomeView onSelectProject={selectProject} />}
          {view === "board" && project && <BoardView project={project} />}
          {view === "list" && project && <ListView project={project} />}
          {view === "timeline" && project && <TimelineView project={project} />}
        </main>
      </div>
    </ApiContext.Provider>
  );
}
