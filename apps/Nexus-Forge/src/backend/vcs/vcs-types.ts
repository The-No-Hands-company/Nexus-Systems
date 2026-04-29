export interface VCSCommit {
  hash: string;
  author: string;
  message: string;
  timestamp: string;
  files: string[];
}

export interface VCSDiff {
  oldPath: string;
  newPath: string;
  additions: number;
  deletions: number;
  hunks: string[];
}

export interface VCSBackend {
  name: "git" | "svn" | "hg" | "pijul";
  version: string;

  init(path: string, bare?: boolean): Promise<void>;
  getHead(path: string): Promise<VCSCommit | null>;
  getHistory(path: string, limit?: number, skip?: number): Promise<VCSCommit[]>;
  getCommit(path: string, hash: string): Promise<VCSCommit | null>;
  getDiff(path: string, from: string, to: string): Promise<VCSDiff[]>;
  getFile(path: string, filePath: string, revision?: string): Promise<string | null>;
  listFiles(path: string, revision?: string): Promise<string[]>;
  isValid(path: string): Promise<boolean>;
}
