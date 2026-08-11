import { useState } from "react";
import * as api from "../utils/api";
import { useEditorStore } from "../stores/useEditorStore";
import { saveLastBoardId } from "../utils/persistence";
import type { ElementData } from "../stores/model";

type Status = "idle" | "generating" | "done" | "error";

export default function AIPanel({ boardId }: { boardId: string; currentElements?: ElementData[] }) {
  const [prompt, setPrompt] = useState("");
  const [status, setStatus] = useState<Status>("idle");
  const [detail, setDetail] = useState("");

  const run = async () => {
    if (!prompt.trim() || status === "generating") return;
    setStatus("generating");
    setDetail("");
    try {
      const { elements } = await api.generateDiagram(prompt.trim(), boardId);
      const sb = await api.getBoard(boardId);
      const store = useEditorStore.getState();
      store.setBoard(api.serverBoardToBoardData(sb));
      const newIds = (elements as Array<{ id: string }>).map((e) => e.id);
      store.setSelection(newIds);
      saveLastBoardId(boardId);
      setStatus("done");
      setDetail(`added ${newIds.length} element${newIds.length === 1 ? "" : "s"}`);
    } catch (e) {
      setStatus("error");
      setDetail(e instanceof Error ? e.message : "generation failed");
    }
  };

  return (
    <div className="flex flex-col h-full p-2 gap-2">
      <div className="text-xs font-semibold text-zinc-400 uppercase">AI Generate</div>
      <textarea
        value={prompt}
        onChange={(e) => setPrompt(e.target.value)}
        placeholder="Describe a diagram… e.g. login flow with auth"
        rows={5}
        className="flex-1 min-h-[80px] text-xs bg-zinc-800 rounded p-2 outline-none resize-none"
      />
      <button
        type="button"
        onClick={run}
        disabled={status === "generating" || !prompt.trim()}
        className="px-2 py-1.5 text-xs bg-blue-600 rounded hover:bg-blue-500 disabled:opacity-40"
      >
        {status === "generating" ? "Generating…" : "Generate"}
      </button>
      {status === "done" && <div className="text-xs text-emerald-400">{detail}</div>}
      {status === "error" && <div className="text-xs text-red-400">Error: {detail}</div>}
      {status === "idle" && <div className="text-[11px] text-zinc-600">Generates a diagram flow on the canvas.</div>}
    </div>
  );
}
