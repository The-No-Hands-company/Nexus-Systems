import { useState } from "react";
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

  return (
    <ApiContext.Provider value={api}>
      <div className="flex flex-col h-screen bg-zinc-950 text-zinc-100">
        <header className="flex items-center gap-4 px-4 h-12 border-b border-zinc-800 flex-shrink-0">
          <button
            onClick={goHome}
            className="font-semibold text-sm text-zinc-100 hover:text-white"
          >
            Nexus Project
          </button>

          {project && (
            <>
              <span className="text-zinc-600">/</span>
              <span className="text-sm text-zinc-300">{project.name}</span>
              <nav className="flex gap-1 ml-4">
                {NAV_ITEMS.map(item => (
                  <button
                    key={item.id}
                    onClick={() => setView(item.id)}
                    className={`px-3 py-1 rounded text-sm transition-colors ${
                      view === item.id
                        ? "bg-zinc-700 text-zinc-100"
                        : "text-zinc-400 hover:text-zinc-200"
                    }`}
                  >
                    {item.label}
                  </button>
                ))}
              </nav>
            </>
          )}
        </header>

        <main className="flex-1 overflow-hidden">
          {view === "home" && <HomeView onSelectProject={selectProject} />}
          {view === "board" && project && <BoardView project={project} />}
          {view === "list" && project && <ListView project={project} />}
          {view === "timeline" && project && <TimelineView project={project} />}
        </main>
      </div>
    </ApiContext.Provider>
  );
}
