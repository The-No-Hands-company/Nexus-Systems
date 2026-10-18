import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { randomUUID } from "node:crypto";
import { tmpdir } from "node:os";
import { createServer } from "../src/server";

const ALICE = "user:alice";
const BOB = "user:bob";
type BoardResponse = {
  id: string;
  name: string;
  gridSnap?: boolean;
  background?: string;
  elements: unknown[];
};

function authHeaders(subject: string) {
  return { "content-type": "application/json", "x-nexus-subject": subject };
}

describe("nexus-draw", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    // Bun 1.3.12 cannot bind port 0 in the test sandbox. Use a dedicated
    // loopback port so each fixture can start deterministically.
    process.env.PORT = "43154";
    process.env.NEXUS_DRAW_ENABLE_CLOUD_INTEGRATION = "false";
    process.env.NEXUS_DRAW_DB = `${tmpdir()}/nexus-draw-test-${randomUUID()}.sqlite`;
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-draw");
    expect(body.status).toBe("ok");
    expect(body.phantom).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-draw");
    expect(Array.isArray(body.capabilities)).toBe(true);
  });
});

describe("board routes — auth", () => {
  it("GET /boards without subject returns 401", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards`);
    expect(res.status).toBe(401);
    close();
  });

  it("POST /boards without subject returns 401", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Test" }),
    });
    expect(res.status).toBe(401);
    close();
  });

  it("GET /boards/:id without subject returns 401", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards/some-id`);
    expect(res.status).toBe(401);
    close();
  });
});

describe("board routes — ownership", () => {
  it("PATCH updates board meta", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Meta" }),
      })
    ).json()) as BoardResponse;
    const res = await fetch(`${base}/api/v1/draw/boards/${created.id}`, {
      method: "PATCH",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ gridSnap: true, background: "#000000" }),
    });
    expect(res.status).toBe(200);
    const got = (await (
      await fetch(`${base}/api/v1/draw/boards/${created.id}`, { headers: authHeaders(ALICE) })
    ).json()) as BoardResponse;
    expect(got.gridSnap).toBe(true);
    expect(got.background).toBe("#000000");
    close();
  });

  it("PATCH 404s for a missing board", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards/nope`, {
      method: "PATCH",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ gridSnap: true }),
    });
    expect(res.status).toBe(404);
    close();
  });

  it("DELETE removes a board and 404s the second time", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Temp" }),
      })
    ).json()) as BoardResponse;
    expect(
      (
        await fetch(`${base}/api/v1/draw/boards/${created.id}`, {
          method: "DELETE",
          headers: authHeaders(ALICE),
        })
      ).status,
    ).toBe(200);
    expect(
      (await fetch(`${base}/api/v1/draw/boards/${created.id}`, { headers: authHeaders(ALICE) }))
        .status,
    ).toBe(404);
    close();
  });

  it("IDOR: Bob cannot read Alice's private board", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const board = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Alice secret" }),
      })
    ).json()) as BoardResponse;
    const res = await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      headers: authHeaders(BOB),
    });
    expect(res.status).toBe(403);
    close();
  });

  it("IDOR: Bob cannot PATCH Alice's private board", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const board = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Alice board" }),
      })
    ).json()) as BoardResponse;
    const res = await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      method: "PATCH",
      headers: authHeaders(BOB),
      body: JSON.stringify({ name: "Hacked" }),
    });
    expect(res.status).toBe(404); // engine returns false → 404 (not found for this owner)
    close();
  });

  it("IDOR: Bob cannot DELETE Alice's private board", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const board = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Alice board" }),
      })
    ).json()) as BoardResponse;
    const res = await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      method: "DELETE",
      headers: authHeaders(BOB),
    });
    expect(res.status).toBe(404);
    // Board should still exist for Alice
    const aliceRes = await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      headers: authHeaders(ALICE),
    });
    expect(aliceRes.status).toBe(200);
    close();
  });

  it("IDOR: Bob cannot see Alice's boards in list", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    await fetch(`${base}/api/v1/draw/boards`, {
      method: "POST",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ name: "Alice only board" }),
    });
    const bobBoards = (await (
      await fetch(`${base}/api/v1/draw/boards`, { headers: authHeaders(BOB) })
    ).json()) as Array<Pick<BoardResponse, "name">>;
    expect(bobBoards.map((b) => b.name)).not.toContain("Alice only board");
    close();
  });

  it("Public board is accessible by any authenticated user", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const board = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "Public board" }),
      })
    ).json()) as BoardResponse;
    await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      method: "PATCH",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ isPublic: true }),
    });
    const res = await fetch(`${base}/api/v1/draw/boards/${board.id}`, {
      headers: authHeaders(BOB),
    });
    expect(res.status).toBe(200);
    close();
  });
});

describe("ai routes", () => {
  it("POST /api/v1/draw/ai/generate synthesizes elements", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const r = await fetch(`${base}/api/v1/draw/ai/generate`, {
      method: "POST",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ prompt: "login flow" }),
    });
    expect(r.status).toBe(200);
    const body = (await r.json()) as { elements: { id: string }[] };
    expect(Array.isArray(body.elements)).toBe(true);
    expect(body.elements.length).toBeGreaterThanOrEqual(3);
    close();
  });

  it("POST /api/v1/draw/ai/generate with board_id appends to the board", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const board = (await (
      await fetch(`${base}/api/v1/draw/boards`, {
        method: "POST",
        headers: authHeaders(ALICE),
        body: JSON.stringify({ name: "AI Board" }),
      })
    ).json()) as BoardResponse;
    const before = (await (
      await fetch(`${base}/api/v1/draw/boards/${board.id}`, { headers: authHeaders(ALICE) })
    ).json()) as BoardResponse;
    await fetch(`${base}/api/v1/draw/ai/generate`, {
      method: "POST",
      headers: authHeaders(ALICE),
      body: JSON.stringify({ prompt: "plan", board_id: board.id }),
    });
    const got = (await (
      await fetch(`${base}/api/v1/draw/boards/${board.id}`, { headers: authHeaders(ALICE) })
    ).json()) as BoardResponse;
    expect(got.elements.length).toBeGreaterThanOrEqual(before.elements.length + 3);
    close();
  });

  it("POST /api/v1/draw/ai/generate rejects a missing prompt", async () => {
    const { server, close } = await createServer();
    const r = await fetch(`http://localhost:${server.port}/api/v1/draw/ai/generate`, {
      method: "POST",
      headers: authHeaders(ALICE),
      body: JSON.stringify({}),
    });
    expect(r.status).toBe(400);
    const body = (await r.json()) as { error: string };
    expect(body.error).toBe("prompt is required");
    close();
  });

  it("POST /api/v1/draw/ai/generate without subject returns 401", async () => {
    const { server, close } = await createServer();
    const r = await fetch(`http://localhost:${server.port}/api/v1/draw/ai/generate`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ prompt: "test" }),
    });
    expect(r.status).toBe(401);
    close();
  });
});
