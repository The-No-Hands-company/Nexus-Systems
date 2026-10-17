import { useEffect, useState } from "react";
import Toolbar from "./components/Toolbar";
import TopBar from "./components/TopBar";
import Canvas from "./components/Canvas/Canvas";
import LayerPanel from "./components/LayerPanel";
import PropertiesPanel from "./components/PropertiesPanel";
import BoardPanel from "./components/BoardPanel";
import AIPanel from "./components/AIPanel";
import { useEditorStore } from "./stores/useEditorStore";
import { loadDoc, bootBoard, saveDocDebounced, flushSave, saveLastBoardId, loadLastBoardId } from "./utils/persistence";
import * as api from "./utils/api";
import { connectCollab, type CollabBinding } from "./collab/collab";
import { isEmbedded } from "./embed";

export default function App() {
  const embedded = isEmbedded();
  const [showPanels, setShowPanels] = useState(true);
  const sidebar = useEditorStore((s) => s.sidebar);
  const setSidebar = useEditorStore((s) => s.setSidebar);
  const boardId = useEditorStore((s) => s.boardId);
  const collabBoardId = useEditorStore((s) => s.board?.id);

  const switchBoard = async (id: string) => {
    try {
      const sb = await api.getBoard(id);
      const store = useEditorStore.getState();
      store.setBoard(api.serverBoardToBoardData(sb));
      store.setPan({ x: 0, y: 0 });
      store.setZoom(1);
      saveLastBoardId(id);
    } catch (err) {
      console.error("Failed to switch board:", err);
    }
  };

  const newBoard = async (name: string) => {
    try {
      const b = await api.createBoard(name);
      useEditorStore.getState().setBoard(api.serverBoardToBoardData(b));
      saveLastBoardId(b.id);
    } catch (err) {
      console.error("Failed to create board:", err);
    }
  };

  useEffect(() => {
    const boot = async () => {
      const store = useEditorStore.getState();
      if (store.board) return;
      store.setBoard(bootBoard(loadDoc()));

      try {
        const available = await api.serverAvailable();
        if (!available) return;

        const lastId = loadLastBoardId();
        if (lastId) {
          try {
            const sb = await api.getBoard(lastId);
            store.setBoard(api.serverBoardToBoardData(sb));
            store.setPan({ x: 0, y: 0 });
            store.setZoom(1);
            saveLastBoardId(lastId);
            return;
          } catch {
            // fall through
          }
        }
        const boards = await api.listBoards();
        if (boards.length > 0) {
          const sb = boards[0];
          store.setBoard(api.serverBoardToBoardData(sb));
          store.setPan({ x: 0, y: 0 });
          store.setZoom(1);
          saveLastBoardId(sb.id);
          return;
        }
        const b = await api.createBoard("Untitled Board");
        store.setBoard(api.serverBoardToBoardData(b));
        store.setPan({ x: 0, y: 0 });
        store.setZoom(1);
        saveLastBoardId(b.id);
      } catch (err) {
        console.error("Server boot failed, keeping localStorage board:", err);
      }
    };
    void boot();
  }, []);

  useEffect(() => {
    const unsubscribe = useEditorStore.subscribe(() => {
      const st = useEditorStore.getState();
      if (!st.board) return;
      saveDocDebounced(st.board, st.elements);
    });
    const onHide = () => {
      const st = useEditorStore.getState();
      if (st.board) flushSave(st.board, st.elements);
    };
    window.addEventListener("pagehide", onHide);
    document.addEventListener("visibilitychange", onHide);
    return () => {
      unsubscribe();
      window.removeEventListener("pagehide", onHide);
      document.removeEventListener("visibilitychange", onHide);
    };
  }, []);

  // Live collaboration: bind the current board's yjs room to the store once a
  // server board is loaded. Local store edits flow into the Y.Doc via
  // setElements; remote edits land back in the store via setElementsLive.
  useEffect(() => {
    const activeId = collabBoardId;
    if (!activeId) return;
    let disposed = false;
    let binding: CollabBinding | null = null;
    const unsubscribe = useEditorStore.subscribe(() => {
      if (binding) binding.setElements(useEditorStore.getState().elements);
    });
    connectCollab(activeId, undefined, (els) => {
      const st = useEditorStore.getState();
      st.setElementsLive(els);
      st.setCollabActive(true);
    }).then((b) => {
      if (disposed) {
        b.destroy();
        return;
      }
      binding = b;
      b.setElements(useEditorStore.getState().elements);
    });
    return () => {
      disposed = true;
      unsubscribe();
      binding?.destroy();
      useEditorStore.getState().setCollabActive(false);
    };
  }, [collabBoardId]);

  const boardName = useEditorStore((s) => s.board?.name ?? "none");

  useEffect(() => {
    document.title = boardName !== "none" ? `${boardName} — Draw` : "Draw";
  }, [boardName]);

  return (
    <div className="h-screen flex flex-col">
      <a
        href="#drawing-canvas"
        className="sr-only focus:not-sr-only focus:absolute focus:z-50 focus:top-2 focus:left-2 focus:rounded focus:bg-zinc-800 focus:px-4 focus:py-2 focus:text-zinc-100 focus:ring-2 focus:ring-accent focus:outline-none"
      >
        Skip to drawing canvas
      </a>
      <TopBar embedded={embedded} />
      <div className="flex flex-1 overflow-hidden">
        <Toolbar />
        <main id="drawing-canvas" className="flex-1 relative" aria-label="Drawing area" tabIndex={-1}>
          <Canvas />
        </main>
        {showPanels && (
          <>
            {sidebar === "layers" && (
              <div role="region" aria-label="Layers panel" className="absolute inset-y-0 right-0 z-30 w-56 border-l border-zinc-800 bg-zinc-900 overflow-y-auto sm:relative sm:z-auto sm:inset-auto">
                <LayerPanel />
              </div>
            )}
            {sidebar === "properties" && (
              <div role="region" aria-label="Properties panel" className="absolute inset-y-0 right-0 z-30 w-72 border-l border-zinc-800 bg-zinc-900 overflow-y-auto sm:relative sm:z-auto sm:inset-auto">
                <PropertiesPanel />
              </div>
            )}
            {sidebar === "boards" && (
              <div role="region" aria-label="Boards panel" className="absolute inset-y-0 right-0 z-30 w-64 border-l border-zinc-800 bg-zinc-900 overflow-y-auto sm:relative sm:z-auto sm:inset-auto">
                <BoardPanel key={boardId} onSwitch={switchBoard} onNew={newBoard} />
              </div>
            )}
            {sidebar === "ai" && boardId && (
              <div role="region" aria-label="AI panel" className="absolute inset-y-0 right-0 z-30 w-72 border-l border-zinc-800 bg-zinc-900 overflow-y-auto sm:relative sm:z-auto sm:inset-auto">
                <AIPanel boardId={boardId} />
              </div>
            )}
          </>
        )}
      </div>
      <div role="toolbar" aria-label="Panel controls" className="flex items-center justify-between min-h-[2.5rem] px-3 bg-zinc-900 border-t border-zinc-800 text-xs text-zinc-500 shrink-0 flex-wrap gap-y-1 py-1">
        <div className="flex gap-2 flex-wrap">
          <button
            onClick={() => setSidebar(sidebar === "layers" ? null : "layers")}
            aria-pressed={sidebar === "layers"}
            className={`px-2 py-1 rounded transition-colors hover:text-zinc-300 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent ${sidebar === "layers" ? "text-accent bg-zinc-800" : ""}`}
          >
            Elements
          </button>
          <button
            onClick={() => setSidebar(sidebar === "properties" ? null : "properties")}
            aria-pressed={sidebar === "properties"}
            className={`px-2 py-1 rounded transition-colors hover:text-zinc-300 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent ${sidebar === "properties" ? "text-accent bg-zinc-800" : ""}`}
          >
            Properties
          </button>
          <button
            onClick={() => setSidebar(sidebar === "boards" ? null : "boards")}
            aria-pressed={sidebar === "boards"}
            className={`px-2 py-1 rounded transition-colors hover:text-zinc-300 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent ${sidebar === "boards" ? "text-accent bg-zinc-800" : ""}`}
          >
            Boards
          </button>
          <button
            onClick={() => setSidebar(sidebar === "ai" ? null : "ai")}
            aria-pressed={sidebar === "ai"}
            className={`px-2 py-1 rounded transition-colors hover:text-zinc-300 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent ${sidebar === "ai" ? "text-accent bg-zinc-800" : ""}`}
          >
            AI
          </button>
          <button
            onClick={() => setShowPanels(!showPanels)}
            aria-pressed={showPanels}
            aria-label={showPanels ? "Hide all panels" : "Show all panels"}
            className="px-2 py-1 rounded transition-colors hover:text-zinc-300 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
          >
            {showPanels ? "Hide" : "Show"}
          </button>
        </div>
        <div className="flex gap-3 text-zinc-600" aria-live="polite" aria-atomic="true">
          <span>Board: {boardName}</span>
        </div>
      </div>
    </div>
  );
}