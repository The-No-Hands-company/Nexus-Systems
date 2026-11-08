import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-code", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.PORT = "0";
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-code");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-code");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("Repository, file, and commit operations work", async () => {
    // 1. Create Repository
    const createRes = await fetch(`${base}/api/v1/repos`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: "my-service", description: "A microservice in Nexus" }),
    });
    expect(createRes.status).toBe(201);
    const repo = (await createRes.json()) as any;
    expect(repo.name).toBe("my-service");

    // 2. Save File in Repo
    const fileRes = await fetch(`${base}/api/v1/repos/${repo.id}/files`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        path: "main.ts",
        content: "console.log('hello nexus code');",
        message: "Initial main.ts",
      }),
    });
    expect(fileRes.status).toBe(201);
    const file = (await fileRes.json()) as any;
    expect(file.path).toBe("main.ts");

    // 3. List Files
    const listRes = await fetch(`${base}/api/v1/repos/${repo.id}/files`);
    expect(listRes.status).toBe(200);
    const files = (await listRes.json()) as any[];
    expect(files.some((f) => f.path === "main.ts")).toBe(true);

    // 4. List Commits
    const commitsRes = await fetch(`${base}/api/v1/repos/${repo.id}/commits`);
    expect(commitsRes.status).toBe(200);
    const commits = (await commitsRes.json()) as any[];
    expect(commits.length).toBeGreaterThanOrEqual(1);
    expect(commits[0].message).toContain("Initial main.ts");

    // 4b. Code Search
    const searchRes = await fetch(`${base}/api/v1/repos/${repo.id}/search?q=hello`);
    expect(searchRes.status).toBe(200);
    const searchHits = (await searchRes.json()) as any[];
    expect(searchHits.length).toBeGreaterThanOrEqual(1);
    expect(searchHits[0].path).toBe("main.ts");

    // 4c. Raw File Download
    const rawRes = await fetch(`${base}/api/v1/repos/${repo.id}/raw/main.ts`);
    expect(rawRes.status).toBe(200);
    const rawText = await rawRes.text();
    expect(rawText).toBe("console.log('hello nexus code');");

    // 4d. Archive Download
    const archiveRes = await fetch(`${base}/api/v1/repos/${repo.id}/archive`);
    expect(archiveRes.status).toBe(200);
    const archiveData = (await archiveRes.json()) as any;
    expect(archiveData.repository).toBe("my-service");
    expect(archiveData.files.length).toBeGreaterThanOrEqual(1);

    // 5. Snippet Lifecycle
    const snipRes = await fetch(`${base}/api/v1/snippets`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        title: "Docker run snippet",
        language: "bash",
        code: "docker run -d -p 8080:8080 caddy",
      }),
    });
    expect(snipRes.status).toBe(201);
    const snip = (await snipRes.json()) as any;
    expect(snip.title).toBe("Docker run snippet");

    const delSnip = await fetch(`${base}/api/v1/snippets/${snip.id}`, { method: "DELETE" });
    expect(delSnip.status).toBe(200);
  });
});
