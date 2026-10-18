import type { Server, ServerWebSocket } from "bun";
import * as decoding from "lib0/decoding";
import * as encoding from "lib0/encoding";
import * as awarenessProtocol from "y-protocols/awareness";
import * as syncProtocol from "y-protocols/sync";
import * as Y from "yjs";
import type { DrawEngine } from "./draw-engine";

const messageSync = 0;
const messageAwareness = 1;

type SocketData = { boardId: string };
type CollabSocket = ServerWebSocket<SocketData>;

interface Room {
  doc: Y.Doc;
  conns: Set<CollabSocket>;
  awareness: awarenessProtocol.Awareness;
}

export class CollabServer {
  private rooms = new Map<string, Room>();
  private saveTimers = new Map<string, ReturnType<typeof setTimeout>>();

  constructor(private engine: DrawEngine) {}

  private room(boardId: string): Room {
    const existing = this.rooms.get(boardId);
    if (existing) return existing;

    const doc = new Y.Doc();
    const board = this.engine.getBoard(boardId);
    if (board && Array.isArray(board.elements)) {
      const map = doc.getMap("elements");
      for (const element of board.elements) {
        if (!element || typeof element !== "object" || !("id" in element)) continue;
        const id = element.id;
        if (typeof id === "string") map.set(id, element);
      }
    }
    const awareness = new awarenessProtocol.Awareness(doc);
    const room: Room = { doc, conns: new Set(), awareness };
    doc.on("update", () => this.scheduleSave(boardId, room));
    this.rooms.set(boardId, room);
    return room;
  }

  private scheduleSave(boardId: string, room: Room): void {
    const prev = this.saveTimers.get(boardId);
    if (prev) clearTimeout(prev);
    this.saveTimers.set(
      boardId,
      setTimeout(() => {
        this.saveTimers.delete(boardId);
        const els = this.yMapToArray(room.doc);
        this.engine.updateElements(boardId, els);
      }, 300),
    );
  }

  private yMapToArray(doc: Y.Doc): unknown[] {
    return [...doc.getMap("elements").values()].map((v) => structuredClone(v));
  }

  upgrade(req: Request, server: Server<SocketData>, boardId: string): boolean {
    return server.upgrade(req, { data: { boardId } });
  }

  open(ws: CollabSocket, boardId: string): void {
    const room = this.room(boardId);
    room.conns.add(ws);
    ws.data = { boardId };
    this.sendSyncState(ws, room);
  }

  private sendSyncState(ws: CollabSocket, room: Room): void {
    const encoder = encoding.createEncoder();
    encoding.writeVarUint(encoder, messageSync);
    syncProtocol.writeSyncStep1(encoder, room.doc);
    this.send(ws, encoding.toUint8Array(encoder));
  }

  message(ws: CollabSocket, data: Uint8Array): void {
    const boardId = ws.data.boardId;
    if (!boardId) return;
    const room = this.room(boardId);
    const decoder = decoding.createDecoder(data);
    const type = decoding.readVarUint(decoder);
    if (type === messageSync) {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, messageSync);
      // decoder is already positioned right after the outer messageSync tag,
      // so readSyncMessage reads the sync sub-type next (matching how
      // writeSyncStep1/writeUpdate frame their payloads).
      syncProtocol.readSyncMessage(decoder, encoder, room.doc, ws);
      const out = encoding.toUint8Array(encoder);
      if (encoding.length(encoder) > 1) this.send(ws, out);
      this.broadcastExcept(room, ws);
      return;
    }
    if (type === messageAwareness) {
      const update = decoding.readVarUint8Array(decoder);
      awarenessProtocol.applyAwarenessUpdate(room.awareness, update, ws);
      this.broadcastAwareness(room, ws);
    }
  }

  close(ws: CollabSocket): void {
    const boardId = ws.data.boardId;
    if (!boardId) return;
    const room = this.rooms.get(boardId);
    if (!room) return;
    room.conns.delete(ws);
    if (room.conns.size === 0) {
      this.rooms.delete(boardId);
      const t = this.saveTimers.get(boardId);
      if (t) {
        clearTimeout(t);
        this.saveTimers.delete(boardId);
      }
    }
  }

  private send(ws: CollabSocket, data: Uint8Array): void {
    if (ws.readyState === 1) ws.send(data);
  }

  private broadcastExcept(room: Room, except: CollabSocket): void {
    const update = Y.encodeStateAsUpdate(room.doc);
    const encoder = encoding.createEncoder();
    encoding.writeVarUint(encoder, messageSync);
    encoding.writeVarUint(encoder, syncProtocol.messageYjsUpdate);
    encoding.writeVarUint8Array(encoder, update);
    const framed = encoding.toUint8Array(encoder);
    for (const c of room.conns) if (c !== except) this.send(c, framed);
  }

  private broadcastAwareness(room: Room, except: CollabSocket): void {
    const encoder = encoding.createEncoder();
    encoding.writeVarUint(encoder, messageAwareness);
    encoding.writeVarUint8Array(
      encoder,
      awarenessProtocol.encodeAwarenessUpdate(room.awareness, [
        ...room.awareness.getStates().keys(),
      ]),
    );
    const framed = encoding.toUint8Array(encoder);
    for (const c of room.conns) if (c !== except) this.send(c, framed);
  }
}
