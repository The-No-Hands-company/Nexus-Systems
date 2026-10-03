import { createHash } from "node:crypto";
import { open, readFile, rm, stat } from "node:fs/promises";

/**
 * The ref log: one JSON line per accepted ref update, each carrying the
 * hash of the line before it.
 *
 * What it proves, and to whom:
 *   - editing, dropping or reordering any entry breaks every hash after it
 *     (anyone holding the log can check)
 *   - a ref moved without going through a forge push no longer matches the
 *     state the log replays to (anyone who can also list the refs)
 *   - rewriting the whole chain consistently is caught by a client that
 *     pinned an earlier head: its pinned entry is no longer in the chain
 *
 * It does not stop a compromised server from rewriting history for a client
 * that has never seen it. That needs the head witnessed elsewhere
 * (federation peers), which is the next layer, not this one.
 */
export const GENESIS = "0".repeat(64);
const ZERO_ID = /^0{40}(0{24})?$/;

export interface RefLogEntry {
  seq: number;
  /**
   * seq of the first entry written by the same push. A push is checked as
   * a whole against the state before it, so replaying the policy needs to
   * know where each push starts and ends.
   */
  push: number;
  time: string;
  ref: string;
  old: string;
  new: string;
  pusher: string;
  prev: string;
  hash: string;
}

export function entryHash(entry: Omit<RefLogEntry, "hash">): string {
  const canonical = JSON.stringify([
    entry.seq,
    entry.push,
    entry.time,
    entry.ref,
    entry.old,
    entry.new,
    entry.pusher,
    entry.prev,
  ]);
  return createHash("sha256").update(canonical).digest("hex");
}

export type ChainResult =
  | { ok: true; entries: RefLogEntry[]; head: string; refs: Map<string, string> }
  | { ok: false; error: string };

/** Check every link and replay the log into the ref state it implies. */
export function verifyChain(text: string): ChainResult {
  const entries: RefLogEntry[] = [];
  const refs = new Map<string, string>();
  let prev = GENESIS;
  const lines = text.split("\n").filter((line) => line !== "");
  for (const [index, line] of lines.entries()) {
    let entry: RefLogEntry;
    try {
      entry = JSON.parse(line) as RefLogEntry;
    } catch {
      return { ok: false, error: `entry ${index}: not valid JSON` };
    }
    if (entry.seq !== index) {
      return { ok: false, error: `entry ${index}: sequence number is ${entry.seq}` };
    }
    const startsPush = entry.push === index;
    const continuesPush = index > 0 && entry.push === entries[index - 1]?.push;
    if (!Number.isInteger(entry.push) || !(startsPush || continuesPush)) {
      return { ok: false, error: `entry ${index}: push ${entry.push} is not contiguous` };
    }
    if (entry.prev !== prev) {
      return { ok: false, error: `entry ${index}: does not follow the entry before it` };
    }
    if (entryHash(entry) !== entry.hash) {
      return { ok: false, error: `entry ${index}: contents do not match its hash` };
    }
    const current = refs.get(entry.ref) ?? "0".repeat(entry.old.length);
    if (current !== entry.old) {
      return {
        ok: false,
        error: `entry ${index}: ${entry.ref} moved from a state the log never had`,
      };
    }
    if (ZERO_ID.test(entry.new)) refs.delete(entry.ref);
    else refs.set(entry.ref, entry.new);
    entries.push(entry);
    prev = entry.hash;
  }
  return { ok: true, entries, head: prev, refs };
}

/** Entries grouped into the pushes that wrote them, in order. */
export function groupPushes(entries: RefLogEntry[]): RefLogEntry[][] {
  const pushes: RefLogEntry[][] = [];
  for (const entry of entries) {
    const last = pushes.at(-1);
    if (last && last[0]?.push === entry.push) last.push(entry);
    else pushes.push([entry]);
  }
  return pushes;
}

export interface Pin {
  seq: number;
  hash: string;
}

/** Whether a chain still contains the entry a client verified earlier. */
export function extendsPin(entries: RefLogEntry[], pin: Pin): boolean {
  return entries[pin.seq]?.hash === pin.hash;
}

/**
 * Append ref updates to the log under an exclusive lock, so two pushes
 * finishing together cannot both claim the same sequence number.
 */
export async function appendEntries(
  file: string,
  updates: { ref: string; oldId: string; newId: string }[],
  pusher: string,
  now = new Date(),
): Promise<void> {
  if (updates.length === 0) return;
  const lock = `${file}.lock`;
  await acquire(lock);
  try {
    const existing = await readFile(file, "utf8").catch((error: NodeJS.ErrnoException) => {
      if (error.code === "ENOENT") return "";
      throw error;
    });
    const chain = verifyChain(existing);
    if (!chain.ok) throw new Error(`ref log is already broken (${chain.error}); not appending`);
    let prev = chain.head;
    let seq = chain.entries.length;
    const push = seq;
    const lines: string[] = [];
    for (const update of updates) {
      const body = {
        seq,
        push,
        time: now.toISOString(),
        ref: update.ref,
        old: update.oldId,
        new: update.newId,
        pusher,
        prev,
      };
      const hash = entryHash(body);
      lines.push(JSON.stringify({ ...body, hash }));
      prev = hash;
      seq++;
    }
    const handle = await open(file, "a", 0o644);
    try {
      await handle.write(`${lines.join("\n")}\n`);
      await handle.sync();
    } finally {
      await handle.close();
    }
  } finally {
    await rm(lock, { force: true });
  }
}

async function acquire(lock: string): Promise<void> {
  const deadline = Date.now() + 30_000;
  for (;;) {
    try {
      const handle = await open(lock, "wx");
      await handle.close();
      return;
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== "EEXIST") throw error;
      // A lock older than a minute belongs to a hook that died mid-append.
      const age = await stat(lock).then(
        (s) => Date.now() - s.mtimeMs,
        () => 0,
      );
      if (age > 60_000) await rm(lock, { force: true });
      if (Date.now() > deadline) throw new Error("timed out waiting for the ref log lock");
      await Bun.sleep(20);
    }
  }
}
