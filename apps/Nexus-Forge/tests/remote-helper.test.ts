import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { trustRootFingerprint } from "../src/backend/policy/signers";
import { installHelper } from "../src/cli/remote-helper";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

/**
 * git-remote-nexus: plain git commands, verified. A `nexus::` remote runs
 * the full client verification on every fetch and refuses to bring in
 * anything that fails it.
 */
let honest: TestForge;
let evil: TestForge;
let alice: SigningKey;
let bin: string;
let author: GitClient;

beforeAll(async () => {
  honest = startForge();
  evil = startForge({ compromised: true });
  alice = await makeSigningKey(mkdtempSync(path.join(tmpdir(), "forge-keys-")), "alice@x.test");
  bin = path.join(mkdtempSync(path.join(tmpdir(), "forge-bin-")), "bin");
  installHelper(bin);
  author = new GitClient(mkdtempSync(path.join(tmpdir(), "forge-author-")), "author");
});
afterAll(() => {
  honest.stop();
  evil.stop();
});

/** A developer machine with the helper on PATH and its own verification state. */
function developer(name: string, token = ""): GitClient {
  const home = mkdtempSync(path.join(tmpdir(), `forge-${name}-`));
  return new GitClient(home, name, {
    PATH: `${bin}:${process.env.PATH ?? "/usr/bin:/bin"}`,
    XDG_CONFIG_HOME: path.join(home, "config"),
    XDG_CACHE_HOME: path.join(home, "cache"),
    ...(token ? { NEXUS_FORGE_TOKEN: token } : {}),
  });
}

let counter = 0;
async function publishedRepo(forge: TestForge) {
  const name = `rh-${++counter}`;
  const { user, token } = forge.user(`rh-owner-${counter}`);
  await forge.createRepo(name, user, "public", alice.signerLine);
  const work = await author.initWorkTree(`${name}-src`, alice);
  const pushUrl = remoteUrl(forge, name, token);
  await author.ok(["push", "--quiet", pushUrl, "main"], work);
  return { name, work, token, pushUrl, nexusUrl: `nexus::${remoteUrl(forge, name)}` };
}

const pin = () => `nexus.trustRoot=${trustRootFingerprint(alice.signerLine)}`;

describe("git clone nexus::", () => {
  it("clones a verified repository with plain git", async () => {
    const repo = await publishedRepo(honest);
    const dev = developer("clone");
    await dev.ok(["clone", "--quiet", "-c", pin(), repo.nexusUrl, "c"]);
    expect(existsSync(path.join(dev.home, "c", "README.md"))).toBe(true);
    expect((await dev.ok(["config", "nexus.trustRoot"], path.join(dev.home, "c"))).trim()).toBe(
      trustRootFingerprint(alice.signerLine),
    );
  });

  it("refuses to clone without a trust root, naming the one to check", async () => {
    const repo = await publishedRepo(honest);
    const dev = developer("no-trust");
    const result = await dev.run(["clone", "--quiet", repo.nexusUrl, "c"]);
    expect(result.code).not.toBe(0);
    expect(result.stderr).toContain(trustRootFingerprint(alice.signerLine));
  });
});

describe("git pull from a nexus:: remote", () => {
  it("brings in new verified work", async () => {
    const repo = await publishedRepo(honest);
    const dev = developer("pull");
    await dev.ok(["clone", "--quiet", "-c", pin(), repo.nexusUrl, "c"]);
    const next = await author.commit(repo.work, "b.txt", "b\n", true);
    await author.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);
    const clone = path.join(dev.home, "c");
    await dev.ok(["pull", "--quiet", "--ff-only"], clone);
    expect((await dev.ok(["rev-parse", "HEAD"], clone)).trim()).toBe(next);
  });

  it("refuses what a compromised server let in, and leaves the clone untouched", async () => {
    const repo = await publishedRepo(evil);
    const dev = developer("victim");
    await dev.ok(["clone", "--quiet", "-c", pin(), repo.nexusUrl, "c"]);
    const clone = path.join(dev.home, "c");
    const before = (await dev.ok(["rev-parse", "HEAD"], clone)).trim();

    await author.commit(repo.work, "backdoor.txt", "x\n", false);
    await author.ok(["push", "--quiet", repo.pushUrl, "main"], repo.work);

    const result = await dev.run(["pull", "--ff-only"], clone);
    expect(result.code).not.toBe(0);
    expect(result.stderr).toContain("not signed");
    expect((await dev.ok(["rev-parse", "HEAD"], clone)).trim()).toBe(before);
    expect(existsSync(path.join(clone, "backdoor.txt"))).toBe(false);
  });
});

describe("git push to a nexus:: remote", () => {
  it("pushes signed work through to the server", async () => {
    const repo = await publishedRepo(honest);
    const dev = developer("pusher", repo.token);
    await dev.ok(["clone", "--quiet", "-c", pin(), repo.nexusUrl, "c"]);
    const clone = path.join(dev.home, "c");
    await dev.ok(["config", "user.signingkey", alice.path], clone);
    const pushed = await dev.commit(clone, "c.txt", "c\n", true);
    await dev.ok(["push", "--quiet", "origin", "main"], clone);
    const served = await author.ok(["ls-remote", repo.pushUrl, "refs/heads/main"]);
    expect(served.split("\t")[0]).toBe(pushed);
  });

  it("reports a push the server refuses as failed", async () => {
    const repo = await publishedRepo(honest);
    const dev = developer("unsigned-pusher", repo.token);
    await dev.ok(["clone", "--quiet", "-c", pin(), repo.nexusUrl, "c"]);
    const clone = path.join(dev.home, "c");
    await dev.commit(clone, "u.txt", "u\n", false);
    const result = await dev.run(["push", "origin", "main"], clone);
    expect(result.code).not.toBe(0);
  });
});
