import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { chmodSync, mkdirSync, mkdtempSync, statSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { HOOK_NAMES, assertHooksRunnable, installHooks } from "../src/backend/git/hooks";
import { createForge } from "../src/backend/server";
import {
  GitClient,
  type SigningKey,
  type TestForge,
  makeSigningKey,
  remoteUrl,
  startForge,
} from "./helpers/forge";

/**
 * git silently skips a hook that is not executable. The hook scripts used
 * to be checked in, and git records them as mode 100644 (this repository
 * lives on a filesystem with core.fileMode=false), so on any fresh checkout
 * every push would have been accepted with no signature check and no ref
 * log. Hooks are now generated at startup and probed; a forge whose hooks
 * cannot run must not accept a push.
 */
describe("generated hooks", () => {
  it("installs executable hooks that actually run", () => {
    const dir = path.join(mkdtempSync(path.join(tmpdir(), "forge-hooks-")), "hooks");
    installHooks(dir);
    for (const name of HOOK_NAMES) {
      expect(statSync(path.join(dir, name)).mode & 0o111).not.toBe(0);
    }
    expect(() => assertHooksRunnable(dir)).not.toThrow();
  });

  it("refuses a hooks directory whose hooks are not executable", () => {
    const dir = path.join(mkdtempSync(path.join(tmpdir(), "forge-hooks-")), "hooks");
    installHooks(dir);
    chmodSync(path.join(dir, "pre-receive"), 0o644);
    expect(() => assertHooksRunnable(dir)).toThrow(/pre-receive/);
  });

  it("refuses a hooks directory with a hook missing", () => {
    const dir = path.join(mkdtempSync(path.join(tmpdir(), "forge-hooks-")), "empty");
    mkdirSync(dir, { recursive: true });
    expect(() => assertHooksRunnable(dir)).toThrow();
  });
});

describe("a forge whose hooks stop being runnable", () => {
  let forge: TestForge;
  let token: string;
  let key: SigningKey;

  beforeAll(async () => {
    forge = startForge();
    const owner = forge.user("owner");
    token = owner.token;
    key = await makeSigningKey(path.join(forge.root, "keys"), "owner@example.test");
    await forge.createRepo("guarded", owner.user, "private", key.signerLine);
  });
  afterAll(() => forge.stop());

  it("will not start", () => {
    const dir = path.join(forge.root, "bad-hooks");
    installHooks(dir);
    chmodSync(path.join(dir, "post-receive"), 0o600);
    expect(() => createForge({ db: forge.db, repos: forge.repos, hooksDir: dir })).toThrow();
  });

  it("refuses pushes rather than accepting them unchecked", async () => {
    const git = new GitClient(forge.root, "dev");
    const work = await git.initWorkTree("w", key);
    const url = remoteUrl(forge, "guarded", token);
    await git.ok(["push", "--quiet", url, "main"], work);

    const hooks = path.join(forge.repos.storageRoot, ".forge-hooks");
    chmodSync(path.join(hooks, "pre-receive"), 0o644);
    try {
      // Unsigned: had the hook been skipped, this would land.
      await git.commit(work, "x.txt", "x\n", false);
      const result = await git.run(["push", url, "main"], work);
      expect(result.code).not.toBe(0);
      const remote = await git.ok(["ls-remote", url, "main"]);
      expect(remote.split("\t")[0]).not.toBe((await git.ok(["rev-parse", "HEAD"], work)).trim());
    } finally {
      chmodSync(path.join(hooks, "pre-receive"), 0o755);
    }
  });
});
