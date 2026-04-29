import { VCSBackend, VCSCommit, VCSDiff } from "./vcs-types";

export class SVNBackend implements VCSBackend {
  name: "svn" = "svn";
  version = "1.0.0-stub";

  async init(_path: string, _bare = true): Promise<void> {
    // SVN backend stub.
    // Future implementation: initialize Subversion repository layout.
  }

  async getHead(_path: string): Promise<VCSCommit | null> {
    return null;
  }

  async getHistory(_path: string, _limit = 20, _skip = 0): Promise<VCSCommit[]> {
    return [];
  }

  async getCommit(_path: string, _hash: string): Promise<VCSCommit | null> {
    return null;
  }

  async getDiff(_path: string, _from: string, _to: string): Promise<VCSDiff[]> {
    return [];
  }

  async getFile(_path: string, _filePath: string, _revision?: string): Promise<string | null> {
    return null;
  }

  async listFiles(_path: string, _revision?: string): Promise<string[]> {
    return [];
  }

  async isValid(_path: string): Promise<boolean> {
    return false;
  }
}
