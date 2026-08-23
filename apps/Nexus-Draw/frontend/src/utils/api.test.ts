import { describe, it, expect, vi, beforeEach } from "vitest";

const fetchMock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit): Promise<Response> => {
  const url = String(input);
  const method = init?.method ?? "GET";
  if (method !== "GET") {
    if (url.endsWith("missing")) return new Response(JSON.stringify({ error: "not found" }), { status: 404 });
    if (url === "/api/v1/draw/boards" && method === "POST") return new Response(JSON.stringify({ id: "b1", name: "Test", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean", gridSnap: true, elements: [], collaborators: [], createdAt: "", updatedAt: "" }), { status: 200 });
    if (url === "/api/v1/draw/ai/generate" && method === "POST") return new Response(JSON.stringify({ elements: [{ id: "g1", elementType: "rectangle", data: {}, style: {}, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }], board_id: "b1" }), { status: 200 });
    if (url.match(/\/api\/v1\/draw\/boards\/[^/]+$/) && method === "DELETE") return new Response(JSON.stringify({ deleted: true }), { status: 200 });
    if (url.match(/\/api\/v1\/draw\/boards\/[^/]+\/elements$/) && method === "PUT") return new Response(JSON.stringify({ saved: true }), { status: 200 });
    if (url.match(/\/api\/v1\/draw\/boards\/[^/]+$/) && method === "PATCH") return new Response(JSON.stringify({ updated: true }), { status: 200 });
    return new Response(JSON.stringify({ updated: true }), { status: 200 });
  }
  if (url.endsWith("/health")) return new Response(JSON.stringify({ status: "ok" }), { status: 200 });
  if (url.includes("/boards/")) return new Response(JSON.stringify({ id: "b1", name: "Test", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean", gridSnap: true, elements: [], collaborators: [], createdAt: "", updatedAt: "" }), { status: 200 });
  return new Response(JSON.stringify([{ id: "b1", name: "Test", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean", gridSnap: true, elements: [], collaborators: [], createdAt: "", updatedAt: "" }]), { status: 200 });
});

globalThis.fetch = fetchMock as any;

import {
  listBoards, createBoard, getBoard, saveBoard, deleteBoard, generateDiagram,
  serverAvailable, boardToServerBoard, serverBoardToBoardData,
} from "./api";
import type { BoardData } from "../stores/useEditorStore";

beforeEach(() => {
  fetchMock.mockClear();
});

describe("api", () => {
  it("listBoards hits GET /api/v1/draw/boards", async () => {
    const boards = await listBoards();
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards", expect.objectContaining({ method: "GET" }));
    expect(boards[0].id).toBe("b1");
  });

  it("createBoard POSTs name and returns the board", async () => {
    const b = await createBoard("New");
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards", expect.objectContaining({ method: "POST" }));
    expect(b.id).toBe("b1");
  });

  it("saveBoard PUTs elements and PATCHes meta", async () => {
    const board: BoardData = { id: "b1", name: "Test", description: "", width: 1920, height: 1080, background: "#000", isPublic: false, defaultStyleMode: "clean", gridSnap: true, elements: [] };
    await saveBoard("b1", board, [{ id: "e1", elementType: "rectangle", data: { x: 0, y: 0, width: 1, height: 1 }, style: { stroke: "#fff" } as never, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }]);
    const calls = fetchMock.mock.calls.map((c) => [String(c[0]), (c[1] as RequestInit).method]);
    expect(calls).toContainEqual(["/api/v1/draw/boards/b1/elements", "PUT"]);
    expect(calls).toContainEqual(["/api/v1/draw/boards/b1", "PATCH"]);
  });

  it("deleteBoard DELETEs", async () => {
    await deleteBoard("b1");
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards/b1", expect.objectContaining({ method: "DELETE" }));
  });

  it("generateDiagram POSTs the prompt and returns elements", async () => {
    const res = await generateDiagram("login flow", "b1");
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/ai/generate", expect.objectContaining({ method: "POST" }));
    expect(res.elements.length).toBe(1);
    expect(res.board_id).toBe("b1");
  });

  it("serverAvailable resolves true on /health 200", async () => {
    expect(await serverAvailable()).toBe(true);
  });

  it("converts ServerBoard to BoardData", () => {
    const sb = { id: "b1", name: "N", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "sketch" as const, gridSnap: true, elements: [], collaborators: [], createdAt: "x", updatedAt: "x" };
    const bd = serverBoardToBoardData(sb);
    expect(bd.id).toBe("b1");
    expect(bd.defaultStyleMode).toBe("sketch");
  });

  it("boardToServerBoard copies board + elements", () => {
    const bd: BoardData = { id: "b1", name: "N", description: "", width: 10, height: 10, background: "#000", isPublic: false, defaultStyleMode: "clean", gridSnap: false, elements: [] };
    const sb = boardToServerBoard(bd, [{ id: "e1", elementType: "line", data: { x1: 0, y1: 0, x2: 1, y2: 1 }, style: {} as never, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }]);
    expect(sb.elements.length).toBe(1);
    expect(sb.gridSnap).toBe(false);
  });
});
