import { useEffect, useState } from "react";
import Toolbar from "./components/Toolbar";
import TopBar from "./components/TopBar";
import Canvas from "./components/Canvas/Canvas";
import LayerPanel from "./components/LayerPanel";
import PropertiesPanel from "./components/PropertiesPanel";
import BoardPanel from "./components/BoardPanel";
import { useEditorStore } from "./stores/useEditorStore";
import { loadDoc, bootBoard, saveDocDebounced, flushSave, saveLastBoardId, loadLastBoardId } from "./utils/persistence";
import * as api from "./utils/api";

export default function App() {
  const [showPanels, setShowPanels] = useState(true);
  const sidebar = useEditorStore((s) => s.sidebar);
  const setSidebar = useEditorStore((s) => s.setSidebar);

  const switchBoard = async (id: string) => {
    try {
      const sb = await api.getBoard(id);
      const store = useEditorStore.getState();
      store.setBoard(api.serverBoardToBoardData(sb));
      store.setPan({ x: 0, y: 0 });
      store.setZoom(1);
      saveLastBoardId(id);
    } catch {
      // server board vanished — refresh panel
    }
  };

  const newBoard = async (name: string) => {
    const b = await api.createBoard(name);
    useEditorStore.getState().setBoard(api.serverBoardToBoardData(b));
    saveLastBoardId(b.id);
  };

  useEffect(() => {
    const store = useEditorStore.getState();
    if (store.board) return;
    store.setBoard(bootBoard(loadDoc()));
  }, []);

  useEffect(() => {
    const checkServer = async () => {
      const store = useEditorStore.getState();
      if (store.board) return;
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
    };
    void checkServer();
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

  return (
    <div className="h-screen flex flex-col">
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        <Toolbar />
        <div className="flex-1 relative">
          <Canvas />
        </div>
        {showPanels && (
          <div className="flex">
            {sidebar === "layers" && (
              <div className="w-56 border-l border-zinc-800 bg-zinc-900">
                <LayerPanel />
              </div>
            )}
            {sidebar === "properties" && (
              <div className="w-72 border-l border-zinc-800 bg-zinc-900">
                <PropertiesPanel />
              </div>
            )}
            {sidebar === "boards" && (
              <div className="w-64 border-l border-zinc-800 bg-zinc-900">
                <BoardPanel onSwitch={switchBoard} onNew={newBoard} />
              </div>
            )}
          </div>
        )}
      </div>
      <div className="flex items-center justify-between h-8 px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-600 shrink-0">
        <div className="flex gap-3">
          <button
            onClick={() => setSidebar(sidebar === "layers" ? null : "layers")}
            className={`hover:text-zinc-300 ${sidebar === "layers" ? "text-blue-400" : ""}`}
          >
            Elements
          </button>
          <button
            onClick={() => setSidebar(sidebar === "properties" ? null : "properties")}
            className={`hover:text-zinc-300 ${sidebar === "properties" ? "text-blue-400" : ""}`}
          >
            Properties
          </button>
          <button
            onClick={() => setSidebar(sidebar === "boards" ? null : "boards")}
            className={`hover:text-zinc-300 ${sidebar === "boards" ? "text-blue-400" : ""}`}
          >
            Boards
          </button>
          <button onClick={() => setShowPanels(!showPanels)} className="hover:text-zinc-300">
            {showPanels ? "Hide" : "Show"}
          </button>
        </div>
        <div className="flex gap-3">
          <span>Board: {useEditorStore.getState().board?.name ?? "none"}</span>
        </div>
      </div>
    </div>
  );
}