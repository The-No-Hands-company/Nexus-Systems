import simpleGit, { type SimpleGit, type SimpleGitOptions } from "simple-git";
import type { VCSBackend, VCSCommit, VCSDiff } from "./vcs-types";

const gitOptions: Partial<SimpleGitOptions> = {
  binary: "git",
  maxConcurrentProcesses: 2,
};

export class GitBackend implements VCSBackend {
  name = "git" as const;
  version = "2.40+";

  private git(repoPath: string): SimpleGit {
    return simpleGit({ ...gitOptions, baseDir: repoPath });
  }

  async init(repoPath: string, bare = true): Promise<void> {
    const git = this.git(repoPath);
    await git.init(bare ? ["--bare"] : []);
  }

  async getHead(repoPath: string): Promise<VCSCommit | null> {
    const git = this.git(repoPath);
    const log = await git.log({ maxCount: 1 });
    const latest = log.all[0];
    if (!latest) return null;
    return {
      hash: latest.hash,
      author: latest.author_name,
      message: latest.message,
      timestamp: latest.date,
      files: [],
    };
  }

  async getHistory(repoPath: string, limit = 20, skip = 0): Promise<VCSCommit[]> {
    const git = this.git(repoPath);
    const log = await git.log({ maxCount: limit, skip });
    return log.all.map((item) => ({
      hash: item.hash,
      author: item.author_name,
      message: item.message,
      timestamp: item.date,
      files: [],
    }));
  }

  async getCommit(repoPath: string, hash: string): Promise<VCSCommit | null> {
    const git = this.git(repoPath);
    const result = await git.show(["--quiet", "--format=%H%n%an%n%s%n%ai", hash]);
    if (!result) return null;
    const [commitHashRaw, authorRaw, messageRaw, timestampRaw] = result.trim().split("\n");
    const commitHash = commitHashRaw ?? hash;
    const author = authorRaw ?? "unknown";
    const message = messageRaw ?? "";
    const timestamp = timestampRaw ?? new Date().toISOString();
    return {
      hash: commitHash,
      author,
      message,
      timestamp,
      files: [],
    };
  }

  async getDiff(repoPath: string, from: string, to: string): Promise<VCSDiff[]> {
    const git = this.git(repoPath);
    const raw = await git.diff(["--stat", `${from}..${to}`]);
    return [
      {
        oldPath: from,
        newPath: to,
        additions: 0,
        deletions: 0,
        hunks: [raw],
      },
    ];
  }

  async getFile(repoPath: string, filePath: string, revision = "HEAD"): Promise<string | null> {
    const git = this.git(repoPath);
    try {
      return await git.show([`${revision}:${filePath}`]);
    } catch {
      return null;
    }
  }

  async listFiles(repoPath: string, revision = "HEAD"): Promise<string[]> {
    const git = this.git(repoPath);
    const raw = await git.raw(["ls-tree", "-r", "--name-only", revision]);
    return raw.trim().split("\n").filter(Boolean);
  }

  async isValid(repoPath: string): Promise<boolean> {
    const git = this.git(repoPath);
    try {
      await git.status();
      return true;
    } catch {
      return false;
    }
  }
}
