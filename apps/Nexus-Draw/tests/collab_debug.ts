import { describe, it, expect, beforeEach, afterEach } from "bun:test";
import { createServer } from "../src/server";

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("collab websocket debug", () => {
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

  it("websocket connects and receives sync step 1", { timeout: 30000 }, async () => {
    const ws1 = new WebSocket(`ws://localhost:${server.port}/api/v1/draw/ws/${boardId}`);
    await new Promise<void>((r) => { ws1.onopen = r; });
    await wait(100);
    
    const messages: any[] = [];
    ws1.onmessage = (event) => {
      messages.push(event.data);
    };
    
    await wait(500);
    console.log("Messages received:", messages.length);
    for (const msg of messages) {
      console.log("Message length:", msg.byteLength);
    }
    
    ws1.close();
    expect(messages.length).toBeGreaterThan(0);
  });
});
