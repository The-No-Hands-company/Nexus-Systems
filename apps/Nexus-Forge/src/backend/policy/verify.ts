import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { parseAllowedSigners } from "./signers";

/**
 * The signed-push policy, run by the pre-receive hook.
 *
 * Policy in force for a push: `.nexus/allowed_signers` at the default
 * branch's tip as it stands before the push, or the repository's trust root
 * when that tip has no such file (or there is no tip yet). Every commit the
 * push introduces, on any ref, must carry an SSH signature from a key in it.
 *
 * Reading the policy from the pre-push tip, rather than from each commit's
 * parent, is what makes two attacks fail:
 *   - a commit cannot add its own signer: its own content is not in force yet
 *   - a revoked key stays revoked: branching from a commit made while it was
 *     trusted does not bring the old policy back
 * The cost is that a policy change must land in its own push before the new
 * key can sign anything.
 */
export const POLICY_PATH = ".nexus/allowed_signers";
const ZERO = /^0{40}(0{24})?$/;
const ALLOWED_REF = /^refs\/(heads|tags)\/[^\0]+$/;

export interface RefUpdate {
  oldId: string;
  newId: string;
  ref: string;
}

export type Git = (
  args: string[],
  config?: Record<string, string>,
) => Promise<{
  code: number;
  stdout: string;
  stderr: string;
}>;

export function parseUpdates(input: string): RefUpdate[] {
  return input
    .split("\n")
    .filter((line) => line.trim() !== "")
    .map((line) => {
      const [oldId = "", newId = "", ref = ""] = line.trim().split(" ");
      return { oldId, newId, ref };
    });
}

/** Errors for a push; an empty list means every update is allowed. */
export async function checkPush(
  git: Git,
  updates: RefUpdate[],
  trustRoot: string | null,
): Promise<string[]> {
  const errors: string[] = [];

  const defaultRef = (await git(["symbolic-ref", "HEAD"])).stdout.trim() || "refs/heads/main";
  const tip = await resolveCommit(git, defaultRef);
  let policy: string | null = trustRoot;
  let policySource = "trust root";
  if (tip) {
    const file = await git(["cat-file", "blob", `${tip}:${POLICY_PATH}`]);
    if (file.code === 0) {
      policy = file.stdout;
      policySource = `${POLICY_PATH} at ${defaultRef} ${tip.slice(0, 12)}`;
    }
  }
  const parsed = parseAllowedSigners(policy ?? "");
  if (policy === null || "error" in parsed) {
    return [`no usable signer policy (${policySource}); every push is refused`];
  }

  const dir = await mkdtemp(path.join(tmpdir(), "forge-policy-"));
  const signersFile = path.join(dir, "allowed_signers");
  await writeFile(signersFile, policy);
  const verifyConfig = {
    "gpg.format": "ssh",
    "gpg.ssh.allowedSignersFile": signersFile,
    // A valid signature from a key matching no principal is "Good ...
    // No principal matched." git 2.55 already exits 1 on it (checked by
    // hand); this keeps an older or future git that reports it as an
    // untrusted-but-good signature from turning it into a pass.
    "gpg.minTrustLevel": "fully",
    // Only SSH signatures count; OpenPGP and X.509 ones never verify here.
    "gpg.program": "/bin/false",
    "gpg.x509.program": "/bin/false",
  };

  try {
    for (const update of updates) {
      const { ref, newId } = update;
      if (!ALLOWED_REF.test(ref)) {
        errors.push(`${ref}: only refs/heads/* and refs/tags/* may be pushed`);
        continue;
      }
      if (ZERO.test(newId)) {
        errors.push(`${ref}: deleting refs is not allowed`);
        continue;
      }
      const type = (await git(["cat-file", "-t", newId])).stdout.trim();
      if (ref.startsWith("refs/heads/") && type !== "commit") {
        errors.push(`${ref}: a branch must point at a commit, not a ${type || "missing object"}`);
        continue;
      }
      if (type === "tag") {
        const tag = await git(["verify-tag", newId], verifyConfig);
        if (tag.code !== 0) {
          errors.push(`${ref}: annotated tag is not signed by a key in the ${policySource}`);
          continue;
        }
      } else if (type !== "commit") {
        errors.push(`${ref}: a tag must point at a commit or an annotated tag`);
        continue;
      }

      const list = await git(["rev-list", newId, "--not", "--all"]);
      if (list.code !== 0) {
        errors.push(`${ref}: could not list new commits`);
        continue;
      }
      for (const commit of list.stdout.split("\n").filter(Boolean)) {
        const signed = await git(["cat-file", "commit", commit]);
        if (!/^gpgsig /m.test(signed.stdout.split("\n\n")[0] ?? "")) {
          errors.push(`${ref}: commit ${commit.slice(0, 12)} is not signed`);
          continue;
        }
        const verified = await git(["verify-commit", commit], verifyConfig);
        if (verified.code !== 0) {
          errors.push(
            `${ref}: commit ${commit.slice(0, 12)} is not signed by a key in the ${policySource}`,
          );
        }
      }

      if (ref === defaultRef) {
        const next = await git(["cat-file", "blob", `${newId}:${POLICY_PATH}`]);
        if (next.code === 0) {
          const check = parseAllowedSigners(next.stdout);
          if ("error" in check) {
            errors.push(
              `${ref}: ${POLICY_PATH} is invalid (${check.error}); it would lock the repository`,
            );
          }
        }
      }
    }
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
  return errors;
}

async function resolveCommit(git: Git, ref: string): Promise<string | null> {
  const result = await git(["rev-parse", "--verify", "--quiet", `${ref}^{commit}`]);
  return result.code === 0 ? result.stdout.trim() : null;
}
