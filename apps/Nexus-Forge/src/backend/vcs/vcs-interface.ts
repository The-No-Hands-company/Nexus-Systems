import type { VCSCommit, VCSDiff, VCSBackend } from "./vcs-types";
import { GitBackend } from "./git-backend";
import { SVNBackend } from "./svn-backend";
import { HgBackend } from "./hg-backend";
import { PijulBackend } from "./pijul-backend";

export { VCSCommit, VCSDiff, VCSBackend };

export class VCSFactory {
  static getBackend(type: "git" | "svn" | "hg" | "pijul"): VCSBackend {
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
}
