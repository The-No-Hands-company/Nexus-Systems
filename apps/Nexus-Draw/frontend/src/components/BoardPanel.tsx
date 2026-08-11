import { useEffect, useState } from "react";
import * as api from "../utils/api";
import type { ServerBoard } from "../utils/api";

export default function BoardPanel({ onSwitch, onNew }: { onSwitch: (id: string) => void; onNew: (name: string) => void }) {
  const [boards, setBoards] = useState<ServerBoard[]>([]);
  const [name, setName] = useState("");
  const [loading, setLoading] = useState(true);

  const refresh = async () => { setLoading(true); try { setBoards(await api.listBoards()); } finally { setLoading(false); } };
  useEffect(() => { void refresh(); }, []);

  const create = async () => {
    if (!name.trim()) return;
    const boardName = name.trim();
    setName("");
    onNew(boardName);
    void refresh();
  };

  const remove = async (id: string) => {
    if (!window.confirm("Delete this board?")) return;
    await api.deleteBoard(id);
    void refresh();
  };

  return (
    <div className="flex flex-col h-full p-2 gap-2 overflow-y-auto">
      <div className="text-xs font-semibold text-zinc-400 uppercase">Boards</div>
      {loading && <div className="text-xs text-zinc-600">Loading…</div>}
      {boards.map((b) => (
        <div key={b.id} className="flex items-center gap-1 rounded p-1 bg-zinc-800">
          <div className="flex-1 text-xs text-zinc-200 truncate">{b.name}</div>
          <button onClick={() => onSwitch(b.id)} className="px-1.5 py-0.5 text-[10px] bg-blue-600 rounded hover:bg-blue-500">Open</button>
          <button onClick={() => remove(b.id)} className="px-1.5 py-0.5 text-[10px] bg-zinc-700 rounded hover:bg-red-700">Delete</button>
        </div>
      ))}
      <div className="mt-auto flex gap-1">
        <input value={name} onChange={(e) => setName(e.target.value)} placeholder="New board name" className="flex-1 text-xs bg-zinc-800 rounded px-2 py-1 outline-none" />
        <button onClick={create} className="px-2 py-1 text-xs bg-blue-600 rounded hover:bg-blue-500">Create</button>
      </div>
    </div>
  );
}