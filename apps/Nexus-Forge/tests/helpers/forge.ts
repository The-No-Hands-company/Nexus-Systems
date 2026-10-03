import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { issueToken } from "../../src/backend/auth/tokens";
import { createForge } from "../../src/backend/server";
import { ForgeDB, type UserRecord, type Visibility } from "../../src/backend/storage/db";
import { RepositoryManager } from "../../src/backend/storage/repository";

/** A running forge on a random port, with its own database and storage. */
export interface TestForge {
  url: string;
  root: string;
  db: ForgeDB;
  repos: RepositoryManager;
  fetch: (request: Request) => Promise<Response>;
  user(name: string): { user: UserRecord; token: string };
  createRepo(
    name: string,
    owner: UserRecord,
    visibility: Visibility,
    trustRoot: string,
  ): Promise<void>;
  stop(): void;
}

export function startForge(): TestForge {
  const root = mkdtempSync(path.join(tmpdir(), "forge-test-"));
  const db = new ForgeDB(path.join(root, "forge.db"));
  const repos = new RepositoryManager(path.join(root, "repos"), db);
  const forge = createForge({ db, repos });
  const server = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: forge.fetch });
  return {
    url: `http://127.0.0.1:${server.port}`,
    root,
    db,
    repos,
    fetch: forge.fetch,
    user(name) {
      const user = db.addUser(name);
      return { user, token: issueToken(db, user.id) };
    },
    async createRepo(name, owner, visibility, trustRoot) {
      await repos.createRepository({ name, visibility, trustRoot }, owner);
    },
    stop() {
      server.stop(true);
      db.close();
      rmSync(root, { recursive: true, force: true });
    },
  };
}

export interface SigningKey {
  /** Path to the private key, usable as user.signingkey. */
  path: string;
  /** One allowed_signers line for this key. */
  signerLine: string;
}

/** A fresh ed25519 key, the kind a developer uses for `git commit -S`. */
export async function makeSigningKey(dir: string, principal: string): Promise<SigningKey> {
  mkdirSync(dir, { recursive: true });
  const keyPath = path.join(dir, `${principal.replace(/\W/g, "_")}_ed25519`);
  const gen = Bun.spawnSync([
    "ssh-keygen",
    "-q",
    "-t",
    "ed25519",
    "-N",
    "",
    "-C",
    principal,
    "-f",
    keyPath,
  ]);
  if (gen.exitCode !== 0) throw new Error(`ssh-keygen failed: ${gen.stderr.toString()}`);
  const pub = (await Bun.file(`${keyPath}.pub`).text()).trim().split(/\s+/);
  return { path: keyPath, signerLine: `${principal} namespaces="git" ${pub[0]} ${pub[1]}` };
}

export interface GitResult {
  code: number;
  stdout: string;
  stderr: string;
}

/**
 * A git client in its own HOME, so the developer's real config, credential
 * helpers and signing keys never leak into a test.
 */
export class GitClient {
  readonly home: string;
  private readonly configPath: string;

  constructor(base: string, name: string) {
    this.home = path.join(base, `client-${name}`);
    mkdirSync(this.home, { recursive: true });
    this.configPath = path.join(this.home, ".gitconfig");
    writeFileSync(
      this.configPath,
      [
        "[user]",
        `\tname = ${name}`,
        `\temail = ${name}@example.test`,
        "[init]",
        "\tdefaultBranch = main",
        "[gpg]",
        "\tformat = ssh",
        "[credential]",
        "\thelper =",
        "",
      ].join("\n"),
    );
  }

  async run(args: string[], cwd?: string): Promise<GitResult> {
    const proc = Bun.spawn(["git", ...args], {
      cwd: cwd ?? this.home,
      env: {
        PATH: process.env.PATH ?? "/usr/bin:/bin",
        HOME: this.home,
        GIT_CONFIG_GLOBAL: this.configPath,
        GIT_CONFIG_NOSYSTEM: "1",
        GIT_TERMINAL_PROMPT: "0",
        GIT_ASKPASS: "/bin/false",
        SSH_ASKPASS: "/bin/false",
      },
      stdout: "pipe",
      stderr: "pipe",
    });
    const [stdout, stderr, code] = await Promise.all([
      new Response(proc.stdout).text(),
      new Response(proc.stderr).text(),
      proc.exited,
    ]);
    return { code, stdout, stderr };
  }

  async ok(args: string[], cwd?: string): Promise<string> {
    const result = await this.run(args, cwd);
    if (result.code !== 0) {
      throw new Error(`git ${args.join(" ")} failed (${result.code}):\n${result.stderr}`);
    }
    return result.stdout;
  }

  /** A new working copy with one commit, signed with `key` when given. */
  async initWorkTree(dir: string, key?: SigningKey): Promise<string> {
    const work = path.join(this.home, dir);
    await this.ok(["init", "--quiet", work]);
    if (key) await this.ok(["config", "user.signingkey", key.path], work);
    await this.commit(work, "README.md", "hello\n", key !== undefined);
    return work;
  }

  async commit(work: string, file: string, content: string, sign: boolean): Promise<string> {
    writeFileSync(path.join(work, file), content);
    await this.ok(["add", file], work);
    await this.ok(["commit", "--quiet", sign ? "-S" : "--no-gpg-sign", "-m", `edit ${file}`], work);
    return (await this.ok(["rev-parse", "HEAD"], work)).trim();
  }
}

/** `http://x:<token>@host/name.git`, the form a git client sends Basic auth from. */
export function remoteUrl(forge: TestForge, repo: string, token?: string): string {
  const url = new URL(`${forge.url}/${repo}.git`);
  if (token) {
    url.username = "x";
    url.password = token;
  }
  return url.toString();
}
