import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { trustRootFingerprint } from "../src/backend/policy/signers";
import { type RefLogEntry, entryHash, verifyChain } from "../src/backend/reflog/chain";
import { REF_LOG_FILE, TRUST_ROOT_FILE } from "../src/backend/storage/repository";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

/**
 * `forge verify`: the client re-runs the server's push policy over every
 * push in the ref log, against a mirror of the repository. The point is the
 * compromised server: one that still keeps the log but lets anything in.
 * Log integrity alone cannot see that (log and refs agree); this must.
 */
const CLI = path.join(import.meta.dir, "../src/cli/forge.ts");

let honest: TestForge;
let evil: TestForge;
let alice: SigningKey;
let bob: SigningKey;
let mallory: SigningKey;
let git: GitClient;

beforeAll(async () => {
  honest = startForge();
  evil = startForge({ compromised: true });
  const keys = mkdtempSync(path.join(tmpdir(), "forge-keys-"));
  alice = await makeSigningKey(keys, "alice@example.test");
  bob = await makeSigningKey(keys, "bob@example.test");
  mallory = await makeSigningKey(keys, "mallory@example.test");
  git = new GitClient(mkdtempSync(path.join(tmpdir(), "forge-dev-")), "dev");
});
afterAll(() => {
  honest.stop();
  evil.stop();
});

interface Client {
  config: string;
  cache: string;
}

function freshClient(): Client {
  return {
    config: mkdtempSync(path.join(tmpdir(), "forge-cfg-")),
    cache: mkdtempSync(path.join(tmpdir(), "forge-cache-")),
  };
}

/**
 * Runs `forge verify`. `trust` defaults to alice's fingerprint (every test
 * repository's trust root); null passes none, so the pinned one applies.
 */
async function verify(
  url: string,
  client: Client,
  options: { trust?: string | null; token?: string; extra?: string[] } = {},
) {
  const trust =
    options.trust === undefined ? trustRootFingerprint(alice.signerLine) : options.trust;
  const args = [...(trust ? ["--trust-root", trust] : []), ...(options.extra ?? [])];
  const proc = Bun.spawn([process.execPath, CLI, "verify", url, ...args], {
    env: {
      PATH: process.env.PATH ?? "/usr/bin:/bin",
      HOME: git.home,
      XDG_CONFIG_HOME: client.config,
      XDG_CACHE_HOME: client.cache,
      ...(options.token ? { NEXUS_FORGE_TOKEN: options.token } : {}),
    },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [out, err, code] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  return { code, out: out + err };
}

let counter = 0;
async function newRepo(forge: TestForge, visibility: "public" | "private" = "public") {
  const name = `cv-${++counter}`;
  const { user, token } = forge.user(`owner-${counter}`);
  await forge.createRepo(name, user, visibility, alice.signerLine);
  const work = path.join(git.home, `${name}-${counter}`);
  await git.ok(["init", "--quiet", work]);
  return {
    name,
    work,
    token,
    pushUrl: remoteUrl(forge, name, token),
    url: remoteUrl(forge, name),
  };
}

async function commit(work: string, key: SigningKey | null, file: string, content = `${file}\n`) {
  if (key) await git.ok(["config", "user.signingkey", key.path], work);
  return git.commit(work, file, content, key !== null);
}

async function setPolicy(work: string, signer: SigningKey, keys: SigningKey[]) {
  mkdirSync(path.join(work, ".nexus"), { recursive: true });
  return commit(
    work,
    signer,
    ".nexus/allowed_signers",
    `${keys.map((k) => k.signerLine).join("\n")}\n`,
  );
}

describe("an honest server", () => {
  it("verifies a history with a policy change, branches and a signed tag", async () => {
    const repo = await newRepo(honest);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    await setPolicy(repo.work, alice, [alice, bob]);
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    await commit(repo.work, bob, "b.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    await git.ok(["checkout", "--quiet", "-b", "feature"], repo.work);
    await commit(repo.work, bob, "f.txt");
    await git.ok(["config", "user.signingkey", alice.path], repo.work);
    await git.ok(["tag", "-s", "-m", "release", "v1"], repo.work);
    await git.ok(["push", "--quiet", repo.pushUrl, "feature", "v1"], repo.work);

    const result = await verify(repo.url, freshClient());
    expect(result.out).toContain("ok");
    expect(result.out).toContain("4 pushes");
    expect(result.code).toBe(0);
  });

  it("checks only new pushes on the next run, and still catches a rewrite of old ones", async () => {
    const repo = await newRepo(honest);
    const client = freshClient();
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    expect((await verify(repo.url, client)).code).toBe(0);

    await commit(repo.work, alice, "b.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const second = await verify(repo.url, client);
    expect(second.code).toBe(0);
    expect(second.out).toContain("1 new");

    const file = honest.repos.metaPath(repo.name, REF_LOG_FILE);
    let prev = "0".repeat(64);
    const rewritten = readFileSync(file, "utf8")
      .trim()
      .split("\n")
      .map((line) => {
        const entry = JSON.parse(line) as RefLogEntry;
        const { hash: _drop, ...body } = { ...entry, prev, pusher: "someone-else" };
        const hash = entryHash(body);
        prev = hash;
        return JSON.stringify({ ...body, hash });
      });
    writeFileSync(file, `${rewritten.join("\n")}\n`);
    const third = await verify(repo.url, client);
    expect(third.code).not.toBe(0);
    expect(third.out).toContain("rewritten");
  });

  it("needs a token for a private repository", async () => {
    const repo = await newRepo(honest, "private");
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    expect((await verify(repo.url, freshClient())).code).not.toBe(0);
    expect((await verify(repo.url, freshClient(), { token: repo.token })).code).toBe(0);
  });
});

describe("a compromised server", () => {
  it("lets an unsigned commit in; the log still checks out, but forge verify does not", async () => {
    const repo = await newRepo(evil);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const unsigned = await commit(repo.work, null, "backdoor.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);

    // The log is intact and matches the refs: log integrity alone cannot see this.
    const served = await (await fetch(`${repo.url}/nexus/ref-log`)).text();
    expect(verifyChain(served).ok).toBe(true);
    const result = await verify(repo.url, freshClient());
    expect(result.code).not.toBe(0);
    expect(result.out).toContain(unsigned.slice(0, 12));
    expect(result.out).toContain("not signed");
  });

  it("lets a commit in signed by a key outside the policy", async () => {
    const repo = await newRepo(evil);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    await commit(repo.work, mallory, "m.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const result = await verify(repo.url, freshClient());
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("not signed by a key");
  });

  it("judges a whole push against the state before it, like the server does", async () => {
    // One push: main gains a policy that trusts mallory, and another branch
    // gets a commit signed by mallory. Judged entry by entry, the second
    // update would see mallory already trusted; judged as one push, it must not.
    const repo = await newRepo(evil);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    await git.ok(["checkout", "--quiet", "-b", "zeta"], repo.work);
    await commit(repo.work, mallory, "z.txt");
    await git.ok(["checkout", "--quiet", "main"], repo.work);
    await setPolicy(repo.work, alice, [alice, mallory]);
    await git.ok(["push", "--quiet", repo.pushUrl, "main", "zeta"], repo.work);

    const log = readFileSync(evil.repos.metaPath(repo.name, REF_LOG_FILE), "utf8")
      .trim()
      .split("\n")
      .map((line) => JSON.parse(line) as RefLogEntry);
    // The order that makes per-entry checking lenient: main first, zeta second.
    expect(log.slice(1).map((e) => [e.ref, e.push])).toEqual([
      ["refs/heads/main", 1],
      ["refs/heads/zeta", 1],
    ]);
    const result = await verify(repo.url, freshClient());
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("refs/heads/zeta");
  });

  it("swaps the trust root after a client has seen it", async () => {
    const repo = await newRepo(evil);
    const client = freshClient();
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    expect((await verify(repo.url, client)).code).toBe(0);

    writeFileSync(evil.repos.metaPath(repo.name, TRUST_ROOT_FILE), mallory.signerLine);
    const result = await verify(repo.url, client, { trust: null });
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("trust root changed");
  });

  it("serves a trust root that differs from the one the client was given", async () => {
    const repo = await newRepo(evil);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const expected = path.join(git.home, `trust-${repo.name}`);
    writeFileSync(expected, `${mallory.signerLine}\n`);
    const result = await verify(repo.url, freshClient(), { trust: expected });
    expect(result.code).not.toBe(0);
    expect(result.out).toContain("trust root");
    writeFileSync(expected, alice.signerLine);
    expect((await verify(repo.url, freshClient(), { trust: expected })).code).toBe(0);
  });
});

describe("trust on first use is opt-in", () => {
  it("refuses a first verification without a trust root, and shows the fingerprint to compare", async () => {
    const repo = await newRepo(honest);
    await commit(repo.work, alice, "a.txt");
    await git.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const client = freshClient();
    const first = await verify(repo.url, client, { trust: null });
    expect(first.code).not.toBe(0);
    expect(first.out).toContain(trustRootFingerprint(alice.signerLine));
    const tofu = await verify(repo.url, client, { trust: null, extra: ["--trust-on-first-use"] });
    expect(tofu.code).toBe(0);
    // Pinned now: later runs need nothing.
    expect((await verify(repo.url, client, { trust: null })).code).toBe(0);
  });
});

describe("URLs", () => {
  it("refuses credentials in the URL and plain http to another machine", async () => {
    const repo = await newRepo(honest);
    const withCredentials = await verify(repo.pushUrl, freshClient());
    expect(withCredentials.code).not.toBe(0);
    expect(withCredentials.out).toContain("credentials");
    const remote = await verify("http://forge.example.test/x.git", freshClient());
    expect(remote.code).not.toBe(0);
    expect(remote.out).toContain("https");
  });
});
