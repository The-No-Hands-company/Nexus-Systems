import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import path from "node:path";
import { issueToken } from "../src/backend/auth/tokens";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

let forge: TestForge;
let ownerToken: string;
let readerToken: string;
let strangerToken: string;
let ownerKey: SigningKey;
let owner: GitClient;
let ownerWork: string;

beforeAll(async () => {
  forge = startForge();
  const o = forge.user("owner");
  const r = forge.user("reader");
  const s = forge.user("stranger");
  ownerToken = o.token;
  readerToken = r.token;
  strangerToken = s.token;
  ownerKey = await makeSigningKey(path.join(forge.root, "keys"), "owner@example.test");
  await forge.createRepo("open", o.user, "public", ownerKey.signerLine);
  await forge.createRepo("secret", o.user, "private", ownerKey.signerLine);
  const secret = forge.db.getRepository("secret");
  if (!secret) throw new Error("secret repo missing");
  forge.db.setGrant(secret.id, r.user.id, "read");

  owner = new GitClient(forge.root, "owner");
  ownerWork = await owner.initWorkTree("work", ownerKey);
  await owner.ok(["push", "--quiet", remoteUrl(forge, "open", ownerToken), "main"], ownerWork);
  await owner.ok(["push", "--quiet", remoteUrl(forge, "secret", ownerToken), "main"], ownerWork);
});

afterAll(() => forge.stop());

function raw(pathAndQuery: string, init: RequestInit = {}, token?: string): Promise<Response> {
  const headers = new Headers(init.headers);
  if (token) headers.set("authorization", `Bearer ${token}`);
  return forge.fetch(new Request(`${forge.url}${pathAndQuery}`, { ...init, headers }));
}

describe("clone", () => {
  it("lets anyone clone a public repository", async () => {
    const anon = new GitClient(forge.root, "anon-clone");
    await anon.ok(["clone", "--quiet", remoteUrl(forge, "open"), "c"]);
    expect(await anon.ok(["log", "--format=%s"], path.join(anon.home, "c"))).toContain(
      "edit README.md",
    );
  });

  it("refuses an anonymous clone of a private repository", async () => {
    const anon = new GitClient(forge.root, "anon-private");
    const result = await anon.run(["clone", "--quiet", remoteUrl(forge, "secret"), "c"]);
    expect(result.code).not.toBe(0);
  });

  it("lets a reader clone a private repository and refuses a stranger", async () => {
    const reader = new GitClient(forge.root, "reader-clone");
    await reader.ok(["clone", "--quiet", remoteUrl(forge, "secret", readerToken), "c"]);
    const stranger = new GitClient(forge.root, "stranger-clone");
    const result = await stranger.run([
      "clone",
      "--quiet",
      remoteUrl(forge, "secret", strangerToken),
      "c",
    ]);
    expect(result.code).not.toBe(0);
  });
});

describe("push", () => {
  it("accepts a push from a writer", async () => {
    const head = await owner.commit(ownerWork, "a.txt", "a\n", true);
    await owner.ok(["push", "--quiet", remoteUrl(forge, "secret", ownerToken), "main"], ownerWork);
    const remote = await owner.ok(["ls-remote", remoteUrl(forge, "secret", ownerToken), "main"]);
    expect(remote.split("\t")[0]).toBe(head);
  });

  it("refuses a push from a reader, from anonymous and from a stranger", async () => {
    await owner.commit(ownerWork, "b.txt", "b\n", true);
    for (const url of [
      remoteUrl(forge, "secret", readerToken),
      remoteUrl(forge, "open"),
      remoteUrl(forge, "open", strangerToken),
    ]) {
      const result = await owner.run(["push", "--quiet", url, "main"], ownerWork);
      expect(result.code).not.toBe(0);
    }
  });
});

describe("bypass attempts", () => {
  it("serves only the three smart-protocol endpoints; dumb-protocol paths are 404", async () => {
    for (const p of [
      "/secret.git/info/refs",
      "/secret.git/HEAD",
      "/secret.git/config",
      "/secret.git/objects/info/packs",
      "/open.git/HEAD",
      "/open.git/config",
      "/open.git/info/refs",
    ]) {
      const response = await raw(p, {}, ownerToken);
      expect({ p, status: response.status }).toEqual({ p, status: 404 });
    }
  });

  it("serves the trust root only to callers who can read the repository", async () => {
    expect((await raw("/secret.git/nexus/trust-root")).status).toBe(401);
    expect((await raw("/secret.git/nexus/trust-root", {}, strangerToken)).status).toBe(404);
    const owned = await raw("/secret.git/nexus/trust-root", {}, ownerToken);
    expect(owned.status).toBe(200);
    expect(await owned.text()).toBe(ownerKey.signerLine);
    expect((await raw("/open.git/nexus/trust-root")).status).toBe(200);
    expect((await raw("/open.git/nexus/trust-root?x=1")).status).toBe(404);
  });

  it("requires write for the receive-pack advertisement, not just for the POST", async () => {
    const anon = await raw("/open.git/info/refs?service=git-receive-pack");
    expect(anon.status).toBe(401);
    const reader = await raw("/secret.git/info/refs?service=git-receive-pack", {}, readerToken);
    expect(reader.status).toBe(403);
    const post = await raw("/open.git/git-receive-pack", { method: "POST", body: "0000" });
    expect(post.status).toBe(401);
  });

  it("rejects an ambiguous service parameter instead of guessing which one git will use", async () => {
    const response = await raw(
      "/open.git/info/refs?service=git-upload-pack&service=git-receive-pack",
    );
    expect(response.status).toBe(400);
  });

  it("does not resolve traversal or case variants to another repository", async () => {
    for (const p of [
      "/open.git/../secret.git/info/refs?service=git-upload-pack",
      "/%2e%2e/secret.git/info/refs?service=git-upload-pack",
      "/open%2fsecret.git/info/refs?service=git-upload-pack",
      "/SECRET.git/info/refs?service=git-upload-pack",
      "/secret/info/refs?service=git-upload-pack",
    ]) {
      const response = await raw(p);
      expect([401, 404]).toContain(response.status);
    }
  });

  it("does not reveal whether a private repository exists", async () => {
    const missing = await raw("/nope.git/info/refs?service=git-upload-pack");
    const hidden = await raw("/secret.git/info/refs?service=git-upload-pack");
    expect(missing.status).toBe(401);
    expect(hidden.status).toBe(401);
    const strangerMissing = await raw(
      "/nope.git/info/refs?service=git-upload-pack",
      {},
      strangerToken,
    );
    const strangerHidden = await raw(
      "/secret.git/info/refs?service=git-upload-pack",
      {},
      strangerToken,
    );
    expect(strangerMissing.status).toBe(404);
    expect(strangerHidden.status).toBe(404);
  });

  it("treats an expired token as no token", async () => {
    const owner = forge.db.getUserByName("owner");
    if (!owner) throw new Error("owner missing");
    const expired = issueToken(forge.db, owner.id, 1, Date.now() - 10);
    const response = await raw("/secret.git/info/refs?service=git-upload-pack", {}, expired);
    expect(response.status).toBe(401);
    expect(response.headers.get("www-authenticate")).toContain("Basic");
  });
});

describe("JSON API", () => {
  it("lists only readable repositories", async () => {
    const names = async (token?: string) =>
      ((await (await raw("/api/repos", {}, token)).json()) as { repos: { name: string }[] }).repos
        .map((r) => r.name)
        .sort();
    expect(await names()).toEqual(["open"]);
    expect(await names(strangerToken)).toEqual(["open"]);
    expect(await names(readerToken)).toEqual(["open", "secret"]);
  });

  it("hides a private repository's details from callers who cannot read it", async () => {
    expect((await raw("/api/repos/secret")).status).toBe(404);
    expect((await raw("/api/repos/secret", {}, strangerToken)).status).toBe(404);
    expect((await raw("/api/repos/secret/activity", {}, strangerToken)).status).toBe(404);
    expect((await raw("/api/repos/secret", {}, readerToken)).status).toBe(200);
  });

  it("requires a signed-in caller and a valid name and trust root to create a repository", async () => {
    const create = (body: unknown, token?: string) =>
      raw(
        "/api/repos",
        {
          method: "POST",
          headers: { "content-type": "application/json" },
          body: JSON.stringify(body),
        },
        token,
      );
    const good = { name: "fresh", visibility: "private", trustRoot: ownerKey.signerLine };
    expect((await create(good)).status).toBe(401);
    expect((await create({ ...good, name: "../escape" }, strangerToken)).status).toBe(400);
    expect((await create({ ...good, trustRoot: "" }, strangerToken)).status).toBe(400);
    expect((await create({ ...good, visibility: "world" }, strangerToken)).status).toBe(400);
    expect((await create(good, strangerToken)).status).toBe(201);
    expect((await create(good, strangerToken)).status).toBe(409);
    const repo = await raw("/api/repos/fresh", {}, strangerToken);
    expect(((await repo.json()) as { visibility: string }).visibility).toBe("private");
  });

  it("no longer hands out admin tokens from a login form", async () => {
    const response = await raw("/api/auth/login", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ username: "admin", password: "" }),
    });
    expect(response.status).toBe(404);
  });
});

describe("push size limit", () => {
  it("refuses a request body over the limit, even without a Content-Length", async () => {
    const { createForge } = await import("../src/backend/server");
    const small = createForge({ db: forge.db, repos: forge.repos, maxPushBytes: 1024 });
    const chunk = new Uint8Array(600);
    const body = new ReadableStream<Uint8Array>({
      start(controller) {
        controller.enqueue(chunk);
        controller.enqueue(chunk);
        controller.close();
      },
    });
    const response = await small.fetch(
      new Request(`${forge.url}/secret.git/git-upload-pack`, {
        method: "POST",
        headers: { authorization: `Bearer ${ownerToken}` },
        body,
      }),
    );
    expect(response.status).toBe(413);
  });
});
