import { createHash } from "node:crypto";
import { existsSync } from "node:fs";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { runGit } from "../backend/git/env";
import { trustRootFingerprint } from "../backend/policy/signers";
import { type Git, POLICY_REF, checkUpdates } from "../backend/policy/verify";
import { extendsPin, groupPushes, verifyChain } from "../backend/reflog/chain";

/**
 * forge verify: enforce the push policy on the client.
 *
 * The server's pre-receive hook is the first line; this is the one that does
 * not depend on the server being honest. Everything the server sends (ref
 * log, trust root, objects, refs) is treated as untrusted input. Against a
 * local mirror of the repository it
 *
 *   1. checks the trust root against a fingerprint obtained out of band, or
 *      the one pinned on an earlier run; a first run with neither refuses
 *   2. checks the ref log's chain and that it still contains the head
 *      verified last time
 *   3. checks the log replays to exactly the refs the mirror fetched
 *   4. replays every push in the log through the same checkUpdates() the
 *      server's hook runs, with the state before each push rebuilt from the
 *      log; pushes verified on an earlier run are skipped (2 proves they are
 *      unchanged)
 */
export interface VerifyOptions {
  token: string;
  /** An allowed_signers file, or a `sha256:<hex>` fingerprint, from somewhere other than the server. */
  trustRoot?: string;
  /** Accept the served trust root on a first run. Explicit, because it trusts the server. */
  trustOnFirstUse?: boolean;
  configHome: string;
  cacheHome: string;
}

export type VerifyResult =
  | {
      ok: true;
      /** Bare mirror holding exactly the verified objects and refs. */
      mirror: string;
      refs: Map<string, string>;
      pushes: number;
      checked: number;
      head: string;
    }
  | { ok: false; error: string };

interface VerifiedRecord {
  seq: number;
  hash: string;
  trustRoot: string;
}

/**
 * git talking to a server it does not trust: http(s) transports only (no
 * ext::, file:// or ssh command hooks), no redirects (a redirect could hand
 * the token to another host), and every fetched object fsck'ed.
 */
export const CLIENT_GIT_CONFIG: Record<string, string> = {
  "protocol.allow": "never",
  "protocol.https.allow": "always",
  "protocol.http.allow": "always",
  "http.followRedirects": "false",
  "transfer.fsckObjects": "true",
  "fetch.fsckObjects": "true",
};

const LOOPBACK = new Set(["127.0.0.1", "localhost", "[::1]"]);

/** The URL as the client will use it, or why it is refused. */
export function checkUrl(raw: string): { url: string } | { error: string } {
  let parsed: URL;
  try {
    parsed = new URL(raw);
  } catch {
    return { error: `not a URL: ${raw}` };
  }
  if (parsed.username || parsed.password) {
    return { error: "credentials in the URL would be stored on disk; use NEXUS_FORGE_TOKEN" };
  }
  const local = parsed.protocol === "http:" && LOOPBACK.has(parsed.hostname);
  if (parsed.protocol !== "https:" && !local) {
    return { error: "only https:// URLs (plain http only to this machine)" };
  }
  if (parsed.search || parsed.hash) return { error: "the URL must not have a query or fragment" };
  return { url: parsed.toString().replace(/\/+$/, "") };
}

export async function verifyRepository(raw: string, options: VerifyOptions): Promise<VerifyResult> {
  const fail = (error: string): VerifyResult => ({ ok: false, error });
  const checked = checkUrl(raw);
  if ("error" in checked) return fail(checked.error);
  const { url } = checked;

  const logText = await fetchMeta(url, "ref-log", options.token);
  const trustRootText = await fetchMeta(url, "trust-root", options.token);
  if (logText === null || trustRootText === null) {
    return fail("could not fetch the ref log and trust root (wrong URL, or a token is needed)");
  }

  const stateFile = path.join(options.configHome, "nexus-forge", "verified.json");
  const records = await readRecords(stateFile);
  const record = records[url];
  const served = trustRootFingerprint(trustRootText);
  if (options.trustRoot) {
    const expected = options.trustRoot.startsWith("sha256:")
      ? options.trustRoot
      : trustRootFingerprint(await readFile(options.trustRoot, "utf8"));
    if (expected !== served) {
      return fail(`the served trust root (${served}) does not match the expected ${expected}`);
    }
  } else if (record) {
    if (record.trustRoot !== served) {
      return fail(
        `the trust root changed since this machine last verified the repository (now ${served})`,
      );
    }
  } else if (!options.trustOnFirstUse) {
    return fail(
      `first verification of ${url}: the server's trust root is ${served}. ` +
        "Compare it with the fingerprint the repository owner gave you, then pass " +
        "--trust-root <fingerprint or file> (or --trust-on-first-use to accept it unchecked).",
    );
  }

  const chain = verifyChain(logText);
  if (!chain.ok) return fail(`ref log chain is broken: ${chain.error}`);
  if (record && record.seq >= 0 && !extendsPin(chain.entries, record)) {
    return fail(
      `history was rewritten: entry ${record.seq} no longer has the hash verified earlier`,
    );
  }

  const mirror = path.join(
    options.cacheHome,
    "nexus-forge",
    "mirrors",
    `${createHash("sha256").update(url).digest("hex").slice(0, 16)}.git`,
  );
  const config = {
    ...CLIENT_GIT_CONFIG,
    ...(options.token ? { "http.extraHeader": `Authorization: Bearer ${options.token}` } : {}),
  };
  let synced: Awaited<ReturnType<typeof runGit>>;
  if (existsSync(mirror)) {
    synced = await runGit(["fetch", "--quiet", "--force", "origin"], { cwd: mirror, config });
  } else {
    await mkdir(path.dirname(mirror), { recursive: true });
    synced = await runGit(["clone", "--quiet", "--mirror", "--", url, mirror], { config });
  }
  if (synced.code !== 0) return fail(`could not mirror the repository: ${synced.stderr.trim()}`);

  const git: Git = (args, extra = {}) =>
    runGit(args, { cwd: mirror, config: { ...CLIENT_GIT_CONFIG, ...extra } });
  const listed = await git(["for-each-ref", "--format=%(objectname) %(refname)"]);
  const refs = new Map<string, string>();
  for (const line of listed.stdout.split("\n").filter(Boolean)) {
    const [id = "", ref = ""] = line.split(" ");
    refs.set(ref, id);
  }
  const mismatched = [...new Set([...chain.refs.keys(), ...refs.keys()])]
    .filter((ref) => chain.refs.get(ref) !== refs.get(ref))
    .sort();
  if (mismatched.length > 0) {
    return fail(`refs served do not match the refs the log replays to: ${mismatched.join(", ")}`);
  }

  const state = new Map<string, string>();
  const pushes = groupPushes(chain.entries);
  let newlyChecked = 0;
  for (const [index, push] of pushes.entries()) {
    const first = push[0];
    const last = push.at(-1);
    if (!first || !last) continue;
    if (!(record && last.seq <= record.seq)) {
      const errors = await checkUpdates(
        git,
        push.map((entry) => ({ oldId: entry.old, newId: entry.new, ref: entry.ref })),
        {
          tip: state.get(POLICY_REF) ?? null,
          known: [...state.values()],
          trustRoot: trustRootText,
        },
      );
      newlyChecked++;
      if (errors.length > 0) {
        return fail(
          `push ${index + 1} (log entries ${first.seq}-${last.seq}, by ${first.pusher || "?"} at ${first.time}) breaks the policy:\n  ${errors.join("\n  ")}`,
        );
      }
    }
    for (const entry of push) state.set(entry.ref, entry.new);
  }

  const head = chain.entries.at(-1);
  records[url] = { seq: head?.seq ?? -1, hash: head?.hash ?? "", trustRoot: served };
  await mkdir(path.dirname(stateFile), { recursive: true });
  await writeFile(stateFile, `${JSON.stringify(records, null, 2)}\n`, { mode: 0o600 });
  return { ok: true, mirror, refs, pushes: pushes.length, checked: newlyChecked, head: chain.head };
}

async function fetchMeta(url: string, file: string, token: string): Promise<string | null> {
  const response = await fetch(`${url}/nexus/${file}`, {
    headers: token ? { authorization: `Bearer ${token}` } : {},
    redirect: "error",
  }).catch(() => null);
  return response?.ok ? response.text() : null;
}

async function readRecords(file: string): Promise<Record<string, VerifiedRecord>> {
  try {
    return JSON.parse(await readFile(file, "utf8")) as Record<string, VerifiedRecord>;
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return {};
    throw new Error(`verification state ${file} is unreadable; refusing to continue without it`);
  }
}

/** Client settings from the environment (token, XDG directories). */
export function clientOptions(): Pick<VerifyOptions, "token" | "configHome" | "cacheHome"> {
  const home = process.env.HOME || "/nonexistent";
  return {
    token: process.env.NEXUS_FORGE_TOKEN ?? "",
    configHome: process.env.XDG_CONFIG_HOME || path.join(home, ".config"),
    cacheHome: process.env.XDG_CACHE_HOME || path.join(home, ".cache"),
  };
}
