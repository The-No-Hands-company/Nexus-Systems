import { GitBackend } from "./git-backend";
import { HgBackend } from "./hg-backend";
import { PijulBackend } from "./pijul-backend";
import { SVNBackend } from "./svn-backend";
import type { VCSBackend, VCSCommit, VCSDiff } from "./vcs-types";

export type { VCSCommit, VCSDiff, VCSBackend };

export function getVCSBackend(type: "git" | "svn" | "hg" | "pijul"): VCSBackend {
  switch (type) {
    case "git":
      return new GitBackend();
    case "svn":
      return new SVNBackend();
    case "hg":
      return new HgBackend();
    case "pijul":
      return new PijulBackend();
    default:
      throw new Error(`Unknown VCS type: ${type}`);
  }
}

export const VCSFactory = {
  getBackend: getVCSBackend,
};
