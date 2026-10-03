import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { parseAllowedSigners } from "./signers";

/**
 * The signed-push policy. The server's pre-receive hook runs it on every
 * push; `forge verify` runs the same function on the client over every push
 * in the ref log.
 *
 * Policy in force for a push: `.nexus/allowed_signers` at POLICY_REF as it
 * stands before the push, or the repository's trust root when that commit
 * has no such file (or POLICY_REF does not exist yet). Every commit the push
 * introduces, on any ref, must carry an SSH signature from a key in it.
 *
 *   - a commit cannot add its own signer: its content is not in force yet
 *   - a revoked key stays revoked: branching from a commit made while it was
 *     trusted does not bring the old policy back
 *   - the policy ref is fixed, not read from HEAD: a server that repoints
 *     HEAD at a branch carrying an unreviewed policy change gains nothing
 *
 * Cost: a policy change must land in its own push before its keys can sign.
 */
export const POLICY_REF = "refs/heads/main";
export const POLICY_PATH = ".nexus/allowed_signers";

/** A full object id, SHA-1 or SHA-256. Nothing else reaches git as a revision. */
export const OBJECT_ID = /^(?:[0-9a-f]{40}|[0-9a-f]{64})$/;
const ZERO_ID = /^(?:0{40}|0{64})$/;
const ALLOWED_REF = /^refs\/(?:heads|tags)\/./;

export interface RefUpdate {
  oldId: string;
  newId: string;
  ref: string;
}

export type Git = (
  args: string[],
  config?: Record<string, string>,
) => Promise<{ code: number; stdout: string; stderr: string }>;

export function parseUpdates(input: string): RefUpdate[] {
  return input
    .split("\n")
    .filter((line) => line.trim() !== "")
    .map((line) => {
      const [oldId = "", newId = "", ref = ""] = line.trim().split(" ");
      return { oldId, newId, ref };
    });
}

/**
 * The state a push is judged against: the refs just before it. The server
 * reads it from its live refs; a client replaying the ref log rebuilds it.
 */
export interface PushContext {
  /** POLICY_REF's commit before the push, if it existed. */
  tip: string | null;
  /** Commits reachable before the push ("all": every ref in the repository). */
  known: string[] | "all";
  trustRoot: string | null;
}

/** Errors for a push on the server, judged against its live refs. */
export async function checkPush(
  git: Git,
  updates: RefUpdate[],
  trustRoot: string | null,
): Promise<string[]> {
  const tip = await git(["rev-parse", "--verify", "--quiet", `${POLICY_REF}^{commit}`]);
  return checkUpdates(git, updates, {
    tip: tip.code === 0 ? tip.stdout.trim() : null,
    known: "all",
    trustRoot,
  });
}

/**
 * Errors for a push; an empty list means every update is allowed. Stops at
 * the first violation of each ref: one bad commit refuses the push, and a
 * push of a million unsigned commits costs one git process, not a million.
 */
export async function checkUpdates(
  git: Git,
  updates: RefUpdate[],
  context: PushContext,
): Promise<string[]> {
  const known = context.known === "all" ? [] : context.known;
  // Object ids may come from an untrusted ref log on the client; anything
  // that is not a plain id must never reach git's argument list.
  for (const id of [context.tip ?? "", ...known].filter(Boolean)) {
    if (!OBJECT_ID.test(id)) return [`refusing malformed object id ${JSON.stringify(id)}`];
  }

  let policy = context.trustRoot;
  let source = "trust root";
  if (context.tip) {
    const file = await git(["cat-file", "blob", `${context.tip}:${POLICY_PATH}`]);
    if (file.code === 0) {
      policy = file.stdout;
      source = `${POLICY_PATH} at ${POLICY_REF} ${context.tip.slice(0, 12)}`;
    }
  }
  if (policy === null || "error" in parseAllowedSigners(policy)) {
    return [`no usable signer policy (${source}); every push is refused`];
  }

  const dir = await mkdtemp(path.join(tmpdir(), "forge-policy-"));
  const signers = path.join(dir, "allowed_signers");
  await writeFile(signers, policy, { mode: 0o600 });
  const verify = {
    "gpg.format": "ssh",
    "gpg.ssh.allowedSignersFile": signers,
    // A good signature from a key matching no principal must not pass. git
    // 2.55 already reports it as not trusted; this pins that behaviour.
    "gpg.minTrustLevel": "fully",
    // Only SSH signatures count; OpenPGP and X.509 ones never verify.
    "gpg.program": "/bin/false",
    "gpg.x509.program": "/bin/false",
  };
  // Everything after --end-of-options is a revision, never an option. "Every
  // ref" has to be said before it (--not --all --not leaves the walk
  // positive again); explicit ids follow it as ^<id>.
  const range = (newId: string) =>
    context.known === "all"
      ? ["--not", "--all", "--not", "--end-of-options", newId]
      : ["--end-of-options", newId, ...known.map((id) => `^${id}`)];

  const errors: string[] = [];
  try {
    for (const { ref, newId } of updates) {
      const error = await checkUpdate(git, ref, newId, range, verify, source);
      if (error) errors.push(`${ref}: ${error}`);
    }
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
  return errors;
}

async function checkUpdate(
  git: Git,
  ref: string,
  newId: string,
  range: (newId: string) => string[],
  verify: Record<string, string>,
  source: string,
): Promise<string | null> {
  if (!ALLOWED_REF.test(ref)) return "only refs/heads/* and refs/tags/* may be pushed";
  if (ZERO_ID.test(newId)) return "deleting refs is not allowed";
  if (!OBJECT_ID.test(newId)) return `malformed object id ${JSON.stringify(newId)}`;

  const type = (await git(["cat-file", "-t", newId])).stdout.trim();
  if (type === "tag") {
    if (!ref.startsWith("refs/tags/")) return "a branch must point at a commit";
    if ((await git(["verify-tag", newId], verify)).code !== 0) {
      return `annotated tag is not signed by a key in the ${source}`;
    }
  } else if (type !== "commit") {
    return `must point at a commit or a signed tag, not a ${type || "missing object"}`;
  }

  // %G? is "G" only for a good signature from a key in the signers file;
  // N (none), B (bad), U (untrusted), X/Y/R (expired/revoked) all refuse.
  const log = await git(["log", "--format=%H %G?", ...range(newId)], verify);
  if (log.code !== 0) return "could not list the commits it introduces";
  for (const line of log.stdout.split("\n").filter(Boolean)) {
    const [commit = "", status] = line.split(" ");
    if (status === "N") return `commit ${commit.slice(0, 12)} is not signed`;
    if (status !== "G") {
      return `commit ${commit.slice(0, 12)} is not signed by a key in the ${source}`;
    }
  }

  if (ref === POLICY_REF) {
    const next = await git(["cat-file", "blob", `${newId}:${POLICY_PATH}`]);
    if (next.code === 0) {
      const check = parseAllowedSigners(next.stdout);
      if ("error" in check) {
        return `${POLICY_PATH} is invalid (${check.error}); it would lock the repository`;
      }
    }
  }
  return null;
}
