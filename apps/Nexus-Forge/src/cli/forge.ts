#!/usr/bin/env bun
/**
 * forge — the Nexus Forge command line.
 *
 *   forge verify <repo-url> [--trust-root <allowed_signers file>]
 *       Enforce the push policy on this machine: mirror the repository and
 *       replay every push in its ref log through the same check the
 *       server's hook runs, so a server that skipped its own check is
 *       caught. Pins the trust root and the verified head in
 *       $XDG_CONFIG_HOME/nexus-forge/verified.json; the mirror lives in
 *       $XDG_CACHE_HOME/nexus-forge/mirrors/. Uses $NEXUS_FORGE_TOKEN.
 *
 *   forge log verify <repo-url>
 *       Check a repository's ref log: every hash link, the ref state it
 *       replays to against the refs the server actually serves, and that it
 *       still contains the head this machine verified last time (pinned in
 *       $XDG_CONFIG_HOME/nexus-forge/pins.json). Uses $NEXUS_FORGE_TOKEN for
 *       private repositories.
 *
 *   forge admin user add <name>
 *   forge admin token <user> [--ttl-hours N]
 *   forge admin repo create <name> --owner <user> --trust-root <file> [--public] [--description D]
 *   forge admin grant <repo> <user> <read|write|admin>
 *       Operate on the forge's database and storage directly
 *       ($FORGE_DB_PATH, $FORGE_STORAGE_PATH). Whoever can run these already
 *       holds the files, so the filesystem is the authority here.
 */
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { homedir } from "node:os";
import path from "node:path";
import { issueToken } from "../backend/auth/tokens";
import { type Pin, extendsPin, verifyChain } from "../backend/reflog/chain";
import { type AccessLevel, ForgeDB } from "../backend/storage/db";
import { RepositoryError, RepositoryManager } from "../backend/storage/repository";
import { verifyRepository } from "./verify";

class UsageError extends Error {}

async function main(argv: string[]): Promise<number> {
  const [group, command, ...rest] = argv;
  if (group === "verify") {
    const args = [command, ...rest].filter((a): a is string => a !== undefined);
    const url = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--trust-root");
    if (!url) throw new UsageError("usage: forge verify <repo-url> [--trust-root <file>]");
    const trustRootFile = flag(args, "--trust-root");
    return verifyRepository(url, {
      token: process.env.NEXUS_FORGE_TOKEN ?? "",
      ...(trustRootFile ? { trustRootFile } : {}),
      configHome: process.env.XDG_CONFIG_HOME || path.join(homedir(), ".config"),
      cacheHome: process.env.XDG_CACHE_HOME || path.join(homedir(), ".cache"),
    });
  }
  if (group === "log" && command === "verify") return logVerify(rest);
  if (group === "admin") return admin(command, rest);
  throw new UsageError(
    "usage: forge verify <repo-url> | forge log verify <repo-url> | forge admin <user|token|repo|grant> ...",
  );
}

async function logVerify(args: string[]): Promise<number> {
  const [rawUrl] = args;
  if (!rawUrl) throw new UsageError("usage: forge log verify <repo-url>");
  const url = rawUrl.replace(/\/+$/, "");
  const token = process.env.NEXUS_FORGE_TOKEN ?? "";
  const headers: Record<string, string> = token ? { authorization: `Bearer ${token}` } : {};

  const response = await fetch(`${url}/nexus/ref-log`, { headers });
  if (!response.ok) {
    console.error(`FAIL: could not fetch the ref log (HTTP ${response.status})`);
    return 1;
  }
  const chain = verifyChain(await response.text());
  if (!chain.ok) {
    console.error(`FAIL: ref log chain is broken: ${chain.error}`);
    return 1;
  }

  const served = await lsRemote(url, token);
  if (!served) {
    console.error("FAIL: could not list the repository's refs");
    return 1;
  }
  const mismatches = diffRefs(chain.refs, served);
  if (mismatches.length > 0) {
    console.error("FAIL: refs served do not match the refs the log replays to:");
    for (const line of mismatches) console.error(`  ${line}`);
    return 1;
  }

  const pinFile = path.join(
    process.env.XDG_CONFIG_HOME || path.join(homedir(), ".config"),
    "nexus-forge",
    "pins.json",
  );
  const pins = await readPins(pinFile);
  const pin = pins[url];
  if (pin && !extendsPin(chain.entries, pin)) {
    console.error(
      `FAIL: history was rewritten: entry ${pin.seq} no longer has the hash ${pin.hash} verified earlier`,
    );
    return 1;
  }
  const last = chain.entries.at(-1);
  if (last) {
    pins[url] = { seq: last.seq, hash: last.hash };
    await mkdir(path.dirname(pinFile), { recursive: true });
    await writeFile(pinFile, `${JSON.stringify(pins, null, 2)}\n`);
  }
  console.log(`ok: ${chain.entries.length} entries, head ${chain.head}, ${served.size} refs match`);
  return 0;
}

async function lsRemote(url: string, token: string): Promise<Map<string, string> | null> {
  const env: Record<string, string> = {
    PATH: process.env.PATH ?? "/usr/bin:/bin",
    HOME: process.env.HOME ?? "/nonexistent",
    GIT_TERMINAL_PROMPT: "0",
    GIT_CONFIG_COUNT: "0",
  };
  if (token) {
    // Through the environment rather than argv, so the token is not in `ps`.
    env.GIT_CONFIG_COUNT = "1";
    env.GIT_CONFIG_KEY_0 = "http.extraHeader";
    env.GIT_CONFIG_VALUE_0 = `Authorization: Bearer ${token}`;
  }
  const proc = Bun.spawn(["git", "ls-remote", "--refs", url], {
    env,
    stdout: "pipe",
    stderr: "pipe",
  });
  const [out, code] = await Promise.all([new Response(proc.stdout).text(), proc.exited]);
  if (code !== 0) return null;
  const refs = new Map<string, string>();
  for (const line of out.split("\n")) {
    const [id, ref] = line.split("\t");
    if (id && ref) refs.set(ref, id);
  }
  return refs;
}

function diffRefs(logged: Map<string, string>, served: Map<string, string>): string[] {
  const out: string[] = [];
  for (const ref of new Set([...logged.keys(), ...served.keys()])) {
    const a = logged.get(ref);
    const b = served.get(ref);
    if (a !== b) out.push(`${ref}: log says ${a ?? "absent"}, server has ${b ?? "absent"}`);
  }
  return out.sort();
}

async function readPins(file: string): Promise<Record<string, Pin>> {
  try {
    return JSON.parse(await readFile(file, "utf8")) as Record<string, Pin>;
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return {};
    throw new Error(`pin file ${file} is unreadable; refusing to continue without it`);
  }
}

async function admin(command: string | undefined, args: string[]): Promise<number> {
  const db = new ForgeDB(process.env.FORGE_DB_PATH || "./data/forge.db");
  const repos = new RepositoryManager(process.env.FORGE_STORAGE_PATH || "./data/repos", db);
  try {
    const user = (name: string | undefined) => {
      const found = name ? db.getUserByName(name) : null;
      if (!found) throw new UsageError(`no such user: ${name ?? "(missing)"}`);
      return found;
    };

    if (command === "user" && args[0] === "add" && args[1]) {
      const created = db.addUser(args[1]);
      console.log(`created user ${created.username} (id ${created.id})`);
      return 0;
    }

    if (command === "token" && args[0]) {
      const hours = Number(flag(args, "--ttl-hours") ?? "8");
      const token = issueToken(db, user(args[0]).id, hours * 60 * 60 * 1000);
      console.log(`token for ${args[0]}, valid ${hours}h (shown once):`);
      console.log(token);
      return 0;
    }

    if (command === "repo" && args[0] === "create" && args[1]) {
      const trustRootFile = flag(args, "--trust-root");
      if (!trustRootFile) throw new UsageError("--trust-root <allowed_signers file> is required");
      const description = flag(args, "--description");
      const repo = await repos.createRepository(
        {
          name: args[1],
          visibility: args.includes("--public") ? "public" : "private",
          trustRoot: await readFile(trustRootFile, "utf8"),
          ...(description ? { description } : {}),
        },
        user(flag(args, "--owner")),
      );
      console.log(`created ${repo.visibility} repository ${repo.name}`);
      return 0;
    }

    if (command === "grant" && args.length === 3) {
      const [repoName = "", userName, level = ""] = args;
      if (!["read", "write", "admin"].includes(level)) {
        throw new UsageError("level must be read, write or admin");
      }
      const repo = db.getRepository(repoName);
      if (!repo) throw new UsageError(`no such repository: ${repoName}`);
      db.setGrant(repo.id, user(userName).id, level as AccessLevel);
      console.log(`granted ${level} on ${repoName} to ${userName}`);
      return 0;
    }

    throw new UsageError(
      "usage: forge admin user add <name> | token <user> [--ttl-hours N] | " +
        "repo create <name> --owner <user> --trust-root <file> [--public] | grant <repo> <user> <level>",
    );
  } finally {
    db.close();
  }
}

function flag(args: string[], name: string): string | undefined {
  const at = args.indexOf(name);
  return at >= 0 ? args[at + 1] : undefined;
}

main(process.argv.slice(2)).then(
  (code) => process.exit(code),
  (error) => {
    const known = error instanceof UsageError || error instanceof RepositoryError;
    console.error(known ? (error as Error).message : error);
    process.exit(known ? 2 : 1);
  },
);
