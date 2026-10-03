import { createHash } from "node:crypto";
import { existsSync } from "node:fs";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { runGit } from "../backend/git/env";
import { type Git, checkUpdates } from "../backend/policy/verify";
import { extendsPin, groupPushes, verifyChain } from "../backend/reflog/chain";

/**
 * forge verify: enforce the push policy on the client.
 *
 * The server's pre-receive hook is the first line; this is the one that
 * does not depend on the server being honest. Against a local mirror of the
 * repository it
 *
 *   1. checks the ref log's chain, and that it replays to exactly the refs
 *      the mirror fetched
 *   2. checks the served trust root against the one given (--trust-root) or
 *      the one this machine saw last time
 *   3. checks the log still contains the head verified last time
 *   4. replays every push in the log through the same checkUpdates() the
 *      server's hook runs, with the state before that push reconstructed
 *      from the log
 *
 * A server that skips its own hook can let an unsigned or wrongly signed
 * commit in, and log it so the refs and the log agree; step 4 catches it.
 * Pushes verified on an earlier run are not re-checked (step 3 guarantees
 * they are unchanged).
 */
export interface VerifyOptions {
  token: string;
  trustRootFile?: string;
  configHome: string;
  cacheHome: string;
}

interface VerifiedRecord {
  seq: number;
  hash: string;
  trustRoot: string;
}

export async function verifyRepository(rawUrl: string, options: VerifyOptions): Promise<number> {
  const url = rawUrl.replace(/\/+$/, "");
  const fail = (message: string) => {
    console.error(`FAIL: ${message}`);
    return 1;
  };

  const logText = await fetchMeta(url, "ref-log", options.token);
  const trustRootText = await fetchMeta(url, "trust-root", options.token);
  if (logText === null || trustRootText === null) {
    return fail("could not fetch the ref log and trust root (wrong URL, or a token is needed)");
  }
  const chain = verifyChain(logText);
  if (!chain.ok) return fail(`ref log chain is broken: ${chain.error}`);

  const trustRoot = digest(normaliseSigners(trustRootText));
  if (options.trustRootFile) {
    const expected = normaliseSigners(await readFile(options.trustRootFile, "utf8"));
    if (digest(expected) !== trustRoot) {
      return fail(`the served trust root does not match ${options.trustRootFile}`);
    }
  }

  const stateFile = path.join(options.configHome, "nexus-forge", "verified.json");
  const records = await readRecords(stateFile);
  const record = records[url];
  if (record && record.trustRoot !== trustRoot) {
    return fail("the trust root changed since this machine last verified the repository");
  }
  if (record && record.seq >= 0 && !extendsPin(chain.entries, record)) {
    return fail(
      `history was rewritten: entry ${record.seq} no longer has the hash verified earlier`,
    );
  }

  const mirror = path.join(
    options.cacheHome,
    "nexus-forge",
    "mirrors",
    `${digest(url).slice(0, 16)}.git`,
  );
  const auth: Record<string, string> = options.token
    ? { "http.extraHeader": `Authorization: Bearer ${options.token}` }
    : {};
  const synced = existsSync(mirror)
    ? await runGit(["fetch", "--quiet", "--force", "origin"], { cwd: mirror, config: auth })
    : await (async () => {
        await mkdir(path.dirname(mirror), { recursive: true });
        return runGit(["clone", "--quiet", "--mirror", url, mirror], { config: auth });
      })();
  if (synced.code !== 0) return fail(`could not mirror the repository: ${synced.stderr.trim()}`);

  const git: Git = (args, config = {}) => runGit(args, { cwd: mirror, config });
  const listed = await git(["for-each-ref", "--format=%(objectname) %(refname)"]);
  const mirrored = new Map<string, string>();
  for (const line of listed.stdout.split("\n").filter(Boolean)) {
    const [id = "", ref = ""] = line.split(" ");
    mirrored.set(ref, id);
  }
  const mismatches = [...new Set([...chain.refs.keys(), ...mirrored.keys()])]
    .filter((ref) => chain.refs.get(ref) !== mirrored.get(ref))
    .sort();
  if (mismatches.length > 0) {
    return fail(`refs served do not match the refs the log replays to: ${mismatches.join(", ")}`);
  }

  const defaultRef = (await git(["symbolic-ref", "HEAD"])).stdout.trim() || "refs/heads/main";
  const state = new Map<string, string>();
  const pushes = groupPushes(chain.entries);
  let checked = 0;
  let failed = false;
  for (const [index, push] of pushes.entries()) {
    const last = push.at(-1);
    const first = push[0];
    if (!first || !last) continue;
    const alreadyVerified = record !== undefined && last.seq <= record.seq;
    if (!alreadyVerified) {
      const errors = await checkUpdates(
        git,
        push.map((entry) => ({ oldId: entry.old, newId: entry.new, ref: entry.ref })),
        {
          defaultRef,
          tip: state.get(defaultRef) ?? null,
          known: [...state.values()],
          trustRoot: trustRootText,
        },
      );
      checked++;
      if (errors.length > 0) {
        failed = true;
        console.error(
          `FAIL: push ${index + 1} (log entries ${first.seq}-${last.seq}, by ${first.pusher || "?"} at ${first.time}) breaks the policy:`,
        );
        for (const error of errors) console.error(`  ${error}`);
      }
    }
    for (const entry of push) state.set(entry.ref, entry.new);
  }
  if (failed) return 1;

  const head = chain.entries.at(-1);
  records[url] = { seq: head?.seq ?? -1, hash: head?.hash ?? "", trustRoot };
  await mkdir(path.dirname(stateFile), { recursive: true });
  await writeFile(stateFile, `${JSON.stringify(records, null, 2)}\n`);
  console.log(
    `ok: ${pushes.length} pushes verified (${checked} new), head ${chain.head}, every commit signed under the policy in force`,
  );
  return 0;
}

async function fetchMeta(url: string, file: string, token: string): Promise<string | null> {
  const response = await fetch(`${url}/nexus/${file}`, {
    headers: token ? { authorization: `Bearer ${token}` } : {},
  }).catch(() => null);
  return response?.ok ? response.text() : null;
}

/** Signer lines without comments, blank lines or trailing space, in file order. */
function normaliseSigners(text: string): string {
  return text
    .split("\n")
    .map((line) => line.trim())
    .filter((line) => line !== "" && !line.startsWith("#"))
    .join("\n");
}

function digest(text: string): string {
  return createHash("sha256").update(text).digest("hex");
}

async function readRecords(file: string): Promise<Record<string, VerifiedRecord>> {
  try {
    return JSON.parse(await readFile(file, "utf8")) as Record<string, VerifiedRecord>;
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return {};
    throw new Error(`verification state ${file} is unreadable; refusing to continue without it`);
  }
}
