import { describe, it, expect, beforeEach, afterEach } from "bun:test";
import { createServer } from "../src/server";

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("collab websocket", () => {
  let server: any;
  let close: any;
  let base: string;
  let boardId: string;

  beforeEach(async () => {
    const { server: s, close: c } = await createServer();
    server = s;
    close = c;
    base = `ws://localhost:${server.port}/api/v1/draw/ws`;
    const resp = await fetch(`http://localhost:${server.port}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Collab Test" }) });
    const board = await resp.json();
    boardId = board.id;
  });

  afterEach(async () => {
    if (close) await close();
  });

  it("two clients share element updates through a board", async () => {
    const ws1 = new WebSocket(`${base}/${boardId}`);
    const ws2 = new WebSocket(`${base}/${boardId}`);
    await new Promise<void>((r) => { ws1.onopen = r; });
    await new Promise<void>((r) => { ws2.onopen = r; });
    await wait(100);

    const Y = await import("yjs");
    const syncProtocol = await import("y-protocols/sync");
    const encoding = await import("lib0/encoding");
    const decoding = await import("lib0/decoding");

    // Client A creates doc with one element
    const docA = new Y.Doc();
    docA.getMap("elements").set("e1", { id: "e1", elementType: "rectangle", data: { x: 1 }, style: {}, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 });

    // Helper to send sync message
    const sendSyncStep1 = (ws: WebSocket, doc: Y.Doc) => {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, 0); // messageSync
      syncProtocol.writeSyncStep1(encoder, doc);
      ws.send(encoding.toUint8Array(encoder));
    };
    const sendUpdate = (ws: WebSocket, update: Uint8Array) => {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, 0); // messageSync
      encoding.writeVarUint(encoder, 3); // messageYjsUpdate
      encoding.writeVarUint8Array(encoder, update);
      ws.send(encoding.toUint8Array(encoder));
    };

    // Client A sends step1 + its state update
    sendSyncStep1(ws1, docA);
    await wait(50);
    sendUpdate(ws1, Y.encodeStateAsUpdate(docA));
    await wait(100);

    // Client B starts empty, sends its (empty) state vector
    const docB = new Y.Doc();
    const gotEvents: any[] = [];
    docB.getMap("elements").observe(() => gotEvents.push(docB.getMap("elements").toJSON()));
    sendSyncStep1(ws2, docB);
    await wait(150);

    // Wait for server to debounce-persist
    await wait(500);
    const got = await (await fetch(`http://localhost:${server.port}/api/v1/draw/boards/${boardId}`)).json();
    const serverHasE1 = got.elements.some((e: any) => e.id === "e1");
    const clientBSawE1 = gotEvents.some((m: any) => (m as Record<string, any>).e1 !== undefined);

    ws1.close(); ws2.close();
    expect(serverHasE1).toBe(true);
    expect(clientBSawE1).toBe(true);
  });
});
