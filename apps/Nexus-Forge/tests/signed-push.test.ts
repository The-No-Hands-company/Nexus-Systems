import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { mkdirSync, writeFileSync } from "node:fs";
import path from "node:path";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

/**
 * The signed-push policy, end to end: a real git client signs (or does not
 * sign) commits with real SSH keys and pushes them to a real forge.
 *
 * Policy in force for a push = `.nexus/allowed_signers` at the default
 * branch's tip before the push, or the repository's trust root when that
 * file is absent. Every new commit in the push must be signed by a key in it.
 */
let forge: TestForge;
let token: string;
let alice: SigningKey;
let bob: SigningKey;
let mallory: SigningKey;
let git: GitClient;
let counter = 0;

beforeAll(async () => {
  forge = startForge();
  const owner = forge.user("owner");
  token = owner.token;
  const keys = path.join(forge.root, "keys");
  alice = await makeSigningKey(keys, "alice@example.test");
  bob = await makeSigningKey(keys, "bob@example.test");
  mallory = await makeSigningKey(keys, "mallory@example.test");
  git = new GitClient(forge.root, "dev");
});

afterAll(() => forge.stop());

/** A fresh repository whose trust root is alice alone, and an empty work tree for it. */
async function freshRepo(): Promise<{ name: string; work: string; url: string }> {
  const name = `proj-${++counter}`;
  const owner = forge.db.getUserByName("owner");
  if (!owner) throw new Error("owner missing");
  await forge.createRepo(name, owner, "private", alice.signerLine);
  const work = path.join(git.home, name);
  await git.ok(["init", "--quiet", work]);
  return { name, work, url: remoteUrl(forge, name, token) };
}

async function commitAs(work: string, key: SigningKey | null, file: string, content: string) {
  if (key) await git.ok(["config", "user.signingkey", key.path], work);
  return git.commit(work, file, content, key !== null);
}

async function setPolicy(work: string, signer: SigningKey, keys: SigningKey[]) {
  mkdirSync(path.join(work, ".nexus"), { recursive: true });
  return commitAs(
    work,
    signer,
    ".nexus/allowed_signers",
    `${keys.map((k) => k.signerLine).join("\n")}\n`,
  );
}

async function push(work: string, url: string, refspec = "main") {
  return git.run(["push", "--porcelain", url, refspec], work);
}

async function remoteRef(url: string, ref: string): Promise<string | null> {
  const out = await git.ok(["ls-remote", url, ref]);
  return out.trim() ? (out.split("\t")[0] ?? null) : null;
}

describe("first push", () => {
  it("rejects an unsigned commit and leaves the repository empty", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, null, "a.txt", "a\n");
    const result = await push(work, url);
    expect(result.code).not.toBe(0);
    expect(result.stderr).toContain("not signed");
    expect(await remoteRef(url, "refs/heads/main")).toBeNull();
  });

  it("rejects a commit signed by a key outside the trust root", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, mallory, "a.txt", "a\n");
    const result = await push(work, url);
    expect(result.code).not.toBe(0);
    expect(result.stderr).toContain("is not signed by a key in the trust root");
    expect(await remoteRef(url, "refs/heads/main")).toBeNull();
  });

  it("accepts a commit signed by a trust-root key", async () => {
    const { work, url } = await freshRepo();
    const head = await commitAs(work, alice, "a.txt", "a\n");
    const result = await push(work, url);
    expect(result.stderr).toBe("");
    expect(result.code).toBe(0);
    expect(await remoteRef(url, "refs/heads/main")).toBe(head);
  });
});

describe("in-repository policy", () => {
  it("lets a trusted signer add a key, after which that key may push", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    await setPolicy(work, alice, [alice, bob]);
    expect((await push(work, url)).code).toBe(0);
    const head = await commitAs(work, bob, "b.txt", "b\n");
    expect((await push(work, url)).code).toBe(0);
    expect(await remoteRef(url, "refs/heads/main")).toBe(head);
  });

  it("does not let a commit authorise its own signer", async () => {
    const { work, url } = await freshRepo();
    await setPolicy(work, mallory, [alice, mallory]);
    expect((await push(work, url)).code).not.toBe(0);
  });

  it("does not let a key added earlier in the same push sign later commits", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    expect((await push(work, url)).code).toBe(0);
    await setPolicy(work, alice, [alice, mallory]);
    await commitAs(work, mallory, "m.txt", "m\n");
    const result = await push(work, url);
    expect(result.code).not.toBe(0);
  });

  it("keeps a revoked key revoked on branches cut before the revocation", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    const trustedBob = await setPolicy(work, alice, [alice, bob]);
    expect((await push(work, url)).code).toBe(0);
    await setPolicy(work, alice, [alice]);
    expect((await push(work, url)).code).toBe(0);

    await git.ok(["checkout", "--quiet", "-b", "old", trustedBob], work);
    await commitAs(work, bob, "b.txt", "b\n");
    const result = await push(work, url, "old");
    expect(result.code).not.toBe(0);
    expect(await remoteRef(url, "refs/heads/old")).toBeNull();
  });

  it("checks every new commit, not just the tip: a signed merge cannot carry an unsigned one", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    expect((await push(work, url)).code).toBe(0);
    await git.ok(["checkout", "--quiet", "-b", "side"], work);
    await commitAs(work, null, "evil.txt", "unsigned\n");
    await git.ok(["checkout", "--quiet", "main"], work);
    await commitAs(work, alice, "c.txt", "c\n");
    await git.ok(["config", "user.signingkey", alice.path], work);
    await git.ok(["merge", "--quiet", "-S", "--no-ff", "-m", "merge", "side"], work);
    const result = await push(work, url);
    expect(result.code).not.toBe(0);
  });

  it("refuses a policy file that does not parse, so a typo cannot lock the repository", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    mkdirSync(path.join(work, ".nexus"), { recursive: true });
    await commitAs(work, alice, ".nexus/allowed_signers", "not a signer line\n");
    const result = await push(work, url);
    expect(result.code).not.toBe(0);
    expect(result.stderr).toContain("allowed_signers");
  });
});

describe("ref rules", () => {
  it("rejects force-pushes and deletions even when signed", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    await commitAs(work, alice, "b.txt", "b\n");
    expect((await push(work, url)).code).toBe(0);
    await git.ok(["reset", "--quiet", "--hard", "HEAD~1"], work);
    await commitAs(work, alice, "c.txt", "c\n");
    expect((await git.run(["push", "--force", url, "main"], work)).code).not.toBe(0);
    expect((await push(work, url, ":main")).code).not.toBe(0);
  });

  it("accepts only branches and tags", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    expect((await push(work, url)).code).toBe(0);
    expect((await push(work, url, "main:refs/notes/x")).code).not.toBe(0);
    expect((await push(work, url, "main:refs/pull/1/head")).code).not.toBe(0);
  });

  it("requires annotated tags to be signed; lightweight tags need only a verified commit", async () => {
    const { work, url } = await freshRepo();
    await commitAs(work, alice, "a.txt", "a\n");
    expect((await push(work, url)).code).toBe(0);
    await git.ok(["tag", "light"], work);
    expect((await push(work, url, "refs/tags/light")).code).toBe(0);
    await git.ok(["tag", "--no-sign", "-a", "-m", "unsigned", "plain"], work);
    expect((await push(work, url, "refs/tags/plain")).code).not.toBe(0);
    await git.ok(["tag", "-s", "-m", "signed", "v1"], work);
    expect((await push(work, url, "refs/tags/v1")).code).toBe(0);
  });
});

describe("fail closed", () => {
  it("rejects every push when the trust root is missing", async () => {
    const { name, work, url } = await freshRepo();
    writeFileSync(forge.repos.metaPath(name, "trust-root"), "");
    await commitAs(work, alice, "a.txt", "a\n");
    expect((await push(work, url)).code).not.toBe(0);
  });
});
