import fs from "node:fs/promises";
import path from "node:path";
import { runGit } from "../git/env";
import { isValidRepoName, repoDirName } from "../git/names";
import { parseAllowedSigners } from "../policy/signers";
import type { ForgeDB, RepositoryRecord, UserRecord, Visibility } from "./db";

export interface RepositorySetup {
  name: string;
  description?: string;
  visibility: Visibility;
  /** allowed_signers text: the keys that may sign the first push. */
  trustRoot: string;
}

/** Forge-owned files kept inside each bare repository, out of git's way. */
export const META_DIR = "nexus";
export const TRUST_ROOT_FILE = "trust-root";
export const REF_LOG_FILE = "ref-log.jsonl";

export class RepositoryError extends Error {}

export class RepositoryManager {
  constructor(
    readonly storageRoot: string,
    private db: ForgeDB,
  ) {}

  /** Absolute path of a repository's bare directory, for a validated name only. */
  repoPath(name: string): string {
    return path.join(this.storageRoot, repoDirName(name));
  }

  metaPath(name: string, file: string): string {
    return path.join(this.repoPath(name), META_DIR, file);
  }

  async createRepository(setup: RepositorySetup, owner: UserRecord): Promise<RepositoryRecord> {
    if (!isValidRepoName(setup.name)) {
      throw new RepositoryError("name must match [a-z0-9][a-z0-9_-]{0,63}");
    }
    if (setup.visibility !== "public" && setup.visibility !== "private") {
      throw new RepositoryError('visibility must be "public" or "private"');
    }
    const signers = parseAllowedSigners(setup.trustRoot ?? "");
    if ("error" in signers) throw new RepositoryError(`trustRoot: ${signers.error}`);
    if (this.db.getRepository(setup.name)) {
      throw new RepositoryError(`repository ${setup.name} already exists`);
    }

    const repoPath = this.repoPath(setup.name);
    await fs.mkdir(this.storageRoot, { recursive: true });
    // `mkdir` without `recursive` fails if the directory exists, so two
    // concurrent creates cannot both initialise the same path.
    await fs.mkdir(repoPath);
    const init = await runGit(["init", "--quiet", "--bare", "--initial-branch=main", repoPath]);
    if (init.code !== 0) {
      await fs.rm(repoPath, { recursive: true, force: true });
      throw new Error(`git init failed: ${init.stderr}`);
    }
    await fs.mkdir(path.join(repoPath, META_DIR));
    await fs.writeFile(this.metaPath(setup.name, TRUST_ROOT_FILE), setup.trustRoot, {
      mode: 0o644,
    });

    const record = this.db.addRepository(
      setup.name,
      setup.visibility,
      owner.id,
      setup.description ?? null,
    );
    this.db.logActivity(record.id, "repository.created", owner.id, `created ${setup.name}`);
    return record;
  }
}
