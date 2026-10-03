import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { trustRootFingerprint } from "../src/backend/policy/signers";
import type { RefLogEntry } from "../src/backend/reflog/chain";
import { REF_LOG_FILE } from "../src/backend/storage/repository";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

const CLI = path.join(import.meta.dir, "../src/cli/forge.ts");

let forge: TestForge;
let token: string;
let key: SigningKey;
let git: GitClient;

beforeAll(async () => {
  forge = startForge();
  const owner = forge.user("owner");
  token = owner.token;
  key = await makeSigningKey(path.join(forge.root, "keys"), "owner@example.test");
  git = new GitClient(forge.root, "dev");
});
afterAll(() => forge.stop());

async function cli(args: string[], env: Record<string, string> = {}) {
  const proc = Bun.spawn([process.execPath, CLI, ...args], {
    env: {
      PATH: process.env.PATH ?? "/usr/bin:/bin",
      HOME: git.home,
      XDG_CONFIG_HOME: env.XDG_CONFIG_HOME ?? mkdtempSync(path.join(tmpdir(), "forge-cfg-")),
      ...env,
    },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [stdout, stderr, code] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  return { code, out: stdout + stderr };
}

let counter = 0;
async function repoWithPushes(visibility: "public" | "private", pushes: number) {
  const name = `log-${++counter}`;
  const owner = forge.db.getUserByName("owner");
  if (!owner) throw new Error("owner missing");
  await forge.createRepo(name, owner, visibility, key.signerLine);
  const work = await git.initWorkTree(name, key);
  const url = remoteUrl(forge, name, token);
  await git.ok(["push", "--quiet", url, "main"], work);
  for (let i = 1; i < pushes; i++) {
    await git.commit(work, `f${i}.txt`, `${i}\n`, true);
    await git.ok(["push", "--quiet", url, "main"], work);
  }
  return { name, work, url, plainUrl: remoteUrl(forge, name) };
}

function readLog(name: string): RefLogEntry[] {
  return readFileSync(forge.repos.metaPath(name, REF_LOG_FILE), "utf8")
    .trim()
    .split("\n")
    .map((line) => JSON.parse(line) as RefLogEntry);
}

function writeLog(name: string, entries: RefLogEntry[]) {
  writeFileSync(
    forge.repos.metaPath(name, REF_LOG_FILE),
    `${entries.map((e) => JSON.stringify(e)).join("\n")}\n`,
  );
}

describe("ref log on push", () => {
  it("records each accepted update with the pusher, chained", async () => {
    const { name } = await repoWithPushes("public", 3);
    const log = readLog(name);
    expect(log.map((e) => [e.seq, e.ref, e.pusher])).toEqual([
      [0, "refs/heads/main", "owner"],
      [1, "refs/heads/main", "owner"],
      [2, "refs/heads/main", "owner"],
    ]);
    expect(log[1]?.prev).toBe(log[0]?.hash ?? "");
  });

  it("records nothing for a refused push", async () => {
    const { name, work, url } = await repoWithPushes("public", 1);
    await git.commit(work, "unsigned.txt", "x\n", false);
    expect((await git.run(["push", url, "main"], work)).code).not.toBe(0);
    expect(readLog(name).length).toBe(1);
  });
});

describe("forge verify against the server's own log", () => {
  // The client-side policy replay is in client-verify.test.ts; these are the
  // log-integrity failures a client must report.
  const verify = (url: string) =>
    cli(["verify", url, "--trust-root", trustRootFingerprint(key.signerLine)], {
      XDG_CACHE_HOME: mkdtempSync(path.join(tmpdir(), "forge-cache-")),
    });

  it("passes for an untouched public repository, anonymously", async () => {
    const { plainUrl } = await repoWithPushes("public", 2);
    const result = await verify(plainUrl);
    expect(result.out).toContain("ok");
    expect(result.code).toBe(0);
  });

  it("fails when an entry was edited on the server", async () => {
    const { name, plainUrl } = await repoWithPushes("public", 2);
    const log = readLog(name);
    writeLog(
      name,
      log.map((e) => (e.seq === 0 ? { ...e, pusher: "someone-else" } : e)),
    );
    const result = await verify(plainUrl);
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("entry 0");
  });

  it("fails when a ref moved without going through a push", async () => {
    const { name, work, plainUrl } = await repoWithPushes("public", 2);
    const first = (await git.ok(["rev-parse", "HEAD~1"], work)).trim();
    const bare = forge.repos.repoPath(name);
    Bun.spawnSync(["git", "--git-dir", bare, "update-ref", "refs/heads/main", first]);
    const result = await verify(plainUrl);
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("do not match");
  });
});

describe("forge admin", () => {
  it("creates a user, token, repository and grant that a git client can then use", async () => {
    const env = {
      FORGE_DB_PATH: path.join(forge.root, "forge.db"),
      FORGE_STORAGE_PATH: forge.repos.storageRoot,
    };
    const trustRoot = path.join(forge.root, "admin-trust-root");
    writeFileSync(trustRoot, `${key.signerLine}\n`);
    expect((await cli(["admin", "user", "add", "carol"], env)).code).toBe(0);
    expect((await cli(["admin", "user", "add", "dave"], env)).code).toBe(0);
    const created = await cli(
      ["admin", "repo", "create", "carols", "--owner", "carol", "--trust-root", trustRoot],
      env,
    );
    expect(created.out).toContain("carols");
    expect(created.out).toContain(trustRootFingerprint(key.signerLine));
    expect(created.code).toBe(0);
    expect((await cli(["admin", "grant", "carols", "dave", "read"], env)).code).toBe(0);
    const issued = await cli(["admin", "token", "dave", "--ttl-hours", "1"], env);
    expect(issued.code).toBe(0);
    const daveToken = issued.out.trim().split("\n").at(-1) ?? "";
    expect(daveToken).toMatch(/^nxf_/);

    const remote = await git.run(["ls-remote", remoteUrl(forge, "carols", daveToken)]);
    expect(remote.code).toBe(0);
    expect((await cli(["admin", "grant", "carols", "dave", "owner"], env)).code).not.toBe(0);
    expect(
      (
        await cli(
          ["admin", "repo", "create", "../x", "--owner", "carol", "--trust-root", trustRoot],
          env,
        )
      ).code,
    ).not.toBe(0);
  });
});
