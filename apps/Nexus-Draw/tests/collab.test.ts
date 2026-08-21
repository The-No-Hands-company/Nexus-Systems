import { afterEach, beforeEach, describe, expect, it } from "bun:test";
import * as decoding from "lib0/decoding";
import * as encoding from "lib0/encoding";
import * as syncProtocol from "y-protocols/sync";
import * as Y from "yjs";
import { createServer } from "../src/server";
import { tmpdir } from "node:os";
import { randomUUID } from "node:crypto";

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("collab websocket", () => {
  let server: any;
  let close: any;
  let boardId: string;

  beforeEach(async () => {
    // Ephemeral port and a private database, for the same reasons as
    // server.test.ts: the live nexus-draw service holds 3075 on this machine,
    // and a hard-coded db path would have this suite writing to real boards.
    process.env["PORT"] = "0";
    process.env["NEXUS_DRAW_DB"] = `${tmpdir()}/nexus-draw-collab-${randomUUID()}.sqlite`;

    const { server: s, close: c } = await createServer();
    server = s;
    close = c;
    const resp = await fetch(`http://localhost:${server.port}/api/v1/draw/boards`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Collab Test" }),
    });
    const board = (await resp.json()) as { id: string };
    boardId = board.id;
  });

  afterEach(async () => {
    if (close) await close();
  });

  it("two clients share element updates through a board", async () => {
    const ws1 = new WebSocket(`ws://localhost:${server.port}/api/v1/draw/ws/${boardId}`);
    const ws2 = new WebSocket(`ws://localhost:${server.port}/api/v1/draw/ws/${boardId}`);
    await new Promise<void>((r) => {
      ws1.onopen = () => r();
    });
    await new Promise<void>((r) => {
      ws2.onopen = () => r();
    });
    await wait(100);

    // Client A creates doc with one element
    const docA = new Y.Doc();
    docA.getMap("elements").set("e1", {
      id: "e1",
      elementType: "rectangle",
      data: { x: 1 },
      style: {},
      transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
      order: 0,
      seed: 1,
    });

    // Client B starts empty, observes its elements map
    const docB = new Y.Doc();
    const gotEvents: unknown[] = [];
    docB.getMap("elements").observe(() => gotEvents.push(docB.getMap("elements").toJSON()));

    const toBytes = (blob: unknown): Uint8Array =>
      blob instanceof ArrayBuffer ? new Uint8Array(blob) : new Uint8Array(blob as ArrayBuffer);

    // Minimal y-sync client: apply incoming messages, reply to step1 with step2
    const wireSync = (ws: WebSocket, doc: Y.Doc) => {
      ws.onmessage = (event: MessageEvent) => {
        const encoder = encoding.createEncoder();
        const decoder = decoding.createDecoder(toBytes(event.data));
        const type = decoding.readVarUint(decoder);
        if (type === 0) {
          syncProtocol.readSyncMessage(decoder, encoder, doc, null);
          if (encoding.length(encoder) > 1) ws.send(encoding.toUint8Array(encoder));
        }
      };
    };
    wireSync(ws1, docA);
    wireSync(ws2, docB);

    const sendSyncStep1 = (ws: WebSocket, doc: Y.Doc) => {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, 0); // messageSync
      syncProtocol.writeSyncStep1(encoder, doc);
      ws.send(encoding.toUint8Array(encoder));
    };
    const sendUpdate = (ws: WebSocket, update: Uint8Array) => {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, 0); // messageSync
      encoding.writeVarUint(encoder, syncProtocol.messageYjsUpdate);
      encoding.writeVarUint8Array(encoder, update);
      ws.send(encoding.toUint8Array(encoder));
    };

    // Client A sends step1 + its state update
    sendSyncStep1(ws1, docA);
    await wait(50);
    sendUpdate(ws1, Y.encodeStateAsUpdate(docA));
    await wait(100);

    // Client B sends its (empty) state vector — server replies with the diff
    sendSyncStep1(ws2, docB);
    await wait(150);

    // Wait for server to debounce-persist
    await wait(500);
    const got = (await (
      await fetch(`http://localhost:${server.port}/api/v1/draw/boards/${boardId}`)
    ).json()) as { elements: { id: string }[] };
    const serverHasE1 = got.elements.some((e) => e.id === "e1");
    const clientBSawE1 = gotEvents.some((m) => (m as Record<string, unknown>).e1 !== undefined);

    ws1.close();
    ws2.close();
    expect(serverHasE1).toBe(true);
    expect(clientBSawE1).toBe(true);
  });
});
