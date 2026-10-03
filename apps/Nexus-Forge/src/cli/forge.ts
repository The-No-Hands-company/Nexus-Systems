#!/usr/bin/env bun
/**
 * forge — the Nexus Forge command line.
 *
 *   forge verify <repo-url> [--trust-root <sha256:fingerprint | file>] [--trust-on-first-use]
 *       Enforce the push policy on this machine (see verify.ts). A first
 *       run needs the trust root's fingerprint from the repository owner.
 *       Uses $NEXUS_FORGE_TOKEN for private repositories.
 *
 *   forge install-helper [--dir <dir>]
 *       Install git-remote-nexus (default ~/.local/bin), after which
 *       `git clone nexus::<repo-url>` and every fetch or pull from that
 *       remote run `forge verify` first and fail if it fails.
 *
 *   forge admin user add <name>
 *   forge admin token <user> [--ttl-hours N]
 *   forge admin repo create <name> --owner <user> --trust-root <file> [--public] [--description D]
 *   forge admin grant <repo> <user> <read|write|admin>
 *       Operate on the forge's database and storage directly
 *       ($FORGE_DB_PATH, $FORGE_STORAGE_PATH). Whoever can run these already
 *       holds the files, so the filesystem is the authority here.
 */
import { readFile } from "node:fs/promises";
import { homedir } from "node:os";
import path from "node:path";
import { issueToken } from "../backend/auth/tokens";
import { trustRootFingerprint } from "../backend/policy/signers";
import { type AccessLevel, ForgeDB } from "../backend/storage/db";
import { RepositoryError, RepositoryManager } from "../backend/storage/repository";
import { installHelper } from "./remote-helper";
import { clientOptions, verifyRepository } from "./verify";

class UsageError extends Error {}

async function main(argv: string[]): Promise<number> {
  const [command, ...args] = argv;
  if (command === "verify") return verify(args);
  if (command === "install-helper") {
    const dir = flag(args, "--dir") ?? path.join(homedir(), ".local", "bin");
    console.log(`installed ${installHelper(dir)}`);
    return 0;
  }
  if (command === "admin") return admin(args[0], args.slice(1));
  throw new UsageError("usage: forge verify <repo-url> | forge install-helper | forge admin ...");
}

async function verify(args: string[]): Promise<number> {
  const trustRoot = flag(args, "--trust-root");
  const url = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--trust-root");
  if (!url) {
    throw new UsageError("usage: forge verify <repo-url> [--trust-root <fingerprint|file>]");
  }
  const result = await verifyRepository(url, {
    ...clientOptions(),
    ...(trustRoot ? { trustRoot } : {}),
    trustOnFirstUse: args.includes("--trust-on-first-use"),
  });
  if (!result.ok) {
    console.error(`FAIL: ${result.error}`);
    return 1;
  }
  console.log(
    `ok: ${result.pushes} pushes verified (${result.checked} new), head ${result.head}, every commit signed under the policy in force`,
  );
  return 0;
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
      const trustRoot = await readFile(trustRootFile, "utf8");
      const description = flag(args, "--description");
      const repo = await repos.createRepository(
        {
          name: args[1],
          visibility: args.includes("--public") ? "public" : "private",
          trustRoot,
          ...(description ? { description } : {}),
        },
        user(flag(args, "--owner")),
      );
      console.log(`created ${repo.visibility} repository ${repo.name}`);
      console.log("trust root fingerprint (give this to collaborators out of band):");
      console.log(trustRootFingerprint(trustRoot));
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
