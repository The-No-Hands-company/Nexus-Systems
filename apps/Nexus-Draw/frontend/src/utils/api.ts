import type { StyleMode } from "../stores/model";
import type { ElementData } from "../stores/model";
import type { BoardData as BoardState } from "../stores/useEditorStore";

export interface ServerBoard {
  id: string; name: string; description: string;
  width: number; height: number; background: string; isPublic: boolean;
  defaultStyleMode: StyleMode; gridSnap: boolean;
  elements: ElementData[]; collaborators: string[];
  createdAt: string; updatedAt: string;
}

async function request(path: string, init?: RequestInit): Promise<Response> {
  return fetch(path, {
    headers: { "content-type": "application/json" },
    method: init?.method ?? "GET",
    ...init,
  });
}

export function boardToServerBoard(b: BoardState, elements: ElementData[]): ServerBoard {
  return {
    id: b.id, name: b.name, description: b.description ?? "",
    width: b.width, height: b.height, background: b.background, isPublic: b.isPublic,
    defaultStyleMode: b.defaultStyleMode, gridSnap: b.gridSnap,
    elements, collaborators: [], createdAt: "", updatedAt: "",
  };
}

export function serverBoardToBoardData(sb: ServerBoard): BoardState {
  return {
    id: sb.id, name: sb.name, description: sb.description ?? "",
    width: sb.width, height: sb.height, background: sb.background, isPublic: sb.isPublic,
    defaultStyleMode: sb.defaultStyleMode, gridSnap: sb.gridSnap, elements: sb.elements,
  };
}

export async function listBoards(): Promise<ServerBoard[]> {
  const r = await request("/api/v1/draw/boards");
  return (await r.json()) as ServerBoard[];
}

export async function createBoard(name: string): Promise<ServerBoard> {
  const r = await request("/api/v1/draw/boards", { method: "POST", body: JSON.stringify({ name }) });
  if (!r.ok) throw new Error(`create board failed: ${r.status}`);
  return (await r.json()) as ServerBoard;
}

export async function getBoard(id: string): Promise<ServerBoard> {
  const r = await request(`/api/v1/draw/boards/${id}`);
  if (!r.ok) throw new Error(`get board failed: ${r.status}`);
  return (await r.json()) as ServerBoard;
}

export async function saveBoard(id: string, board: BoardState, elements: ElementData[]): Promise<void> {
  await request(`/api/v1/draw/boards/${id}/elements`, { method: "PUT", body: JSON.stringify({ elements }) });
  const meta = boardToServerBoard(board, elements);
  await request(`/api/v1/draw/boards/${id}`, {
    method: "PATCH",
    body: JSON.stringify({ name: meta.name, description: meta.description, width: meta.width, height: meta.height, background: meta.background, isPublic: meta.isPublic, defaultStyleMode: meta.defaultStyleMode, gridSnap: meta.gridSnap }),
  });
}

export async function deleteBoard(id: string): Promise<void> {
  await request(`/api/v1/draw/boards/${id}`, { method: "DELETE" });
}

export async function serverAvailable(): Promise<boolean> {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), 1500);
    const r = await fetch("/health", { signal: ctrl.signal });
    clearTimeout(t);
    return r.ok;
  } catch {
    return false;
  }
}