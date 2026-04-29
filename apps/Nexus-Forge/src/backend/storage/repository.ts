import fs from "fs/promises";
import path from "path";
import { VCSFactory } from "../vcs/vcs-interface";
import { ForgeDB, RepositoryRecord } from "./db";

export interface RepositorySetup {
  name: string;
  description?: string;
  vcs: "git" | "svn" | "hg" | "pijul";
}

export class RepositoryManager {
  constructor(private storagePath: string, private db: ForgeDB) {}

  async createRepository(setup: RepositorySetup, ownerId?: number): Promise<void> {
    const repoPath = path.join(this.storagePath, `${setup.name}.${this.getExtension(setup.vcs)}`);
    await fs.mkdir(repoPath, { recursive: true });

    const backend = VCSFactory.getBackend(setup.vcs);
    await backend.init(repoPath, true);

    this.db.addRepository(setup.name, setup.vcs, setup.description, ownerId);
    const record = this.db.getRepository(setup.name);
    if (record) {
      this.db.logActivity(record.id, "repository.created", ownerId, `Created repository ${setup.name}`);
    }
  }

  async deleteRepository(name: string): Promise<void> {
    const repo = this.db.getRepository(name);
    if (!repo) return;
    const repoPath = path.join(this.storagePath, `${name}.${this.getExtension(repo.vcs)}`);
    await fs.rm(repoPath, { recursive: true, force: true });
    // TODO: remove DB entry and permissions
  }

  async getRepository(name: string) {
    const repo = this.db.getRepository(name) as RepositoryRecord | undefined;
    if (!repo) return null;
    return {
      name: repo.name,
      vcs: repo.vcs,
      description: repo.description,
      cloneUrl: `http://localhost:8090/${repo.name}.${this.getExtension(repo.vcs)}`,
      created_at: repo.created_at,
    };
  }

  async listRepositories(limit = 50, skip = 0) {
    const repos = this.db.listRepositories(limit, skip);
    const total = repos.length;
    return { repos, total };
  }

  async getActivity(name: string) {
    const repo = this.db.getRepository(name);
    if (!repo) return [];
    return this.db.getActivity(repo.id, 50);
  }

  private getExtension(vcs: "git" | "svn" | "hg" | "pijul"): string {
    const ext: Record<string, string> = {
      git: "git",
      svn: "svn",
      hg: "hg",
      pijul: "pijul",
    };
    return ext[vcs];
  }
}
