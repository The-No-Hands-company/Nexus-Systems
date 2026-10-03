import { describe, expect, it } from "bun:test";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import {
  type RefLogEntry,
  appendEntries,
  entryHash,
  extendsPin,
  verifyChain,
} from "../src/backend/reflog/chain";

const A = "a".repeat(40);
const B = "b".repeat(40);
const C = "c".repeat(40);
const Z = "0".repeat(40);

async function sampleLog(): Promise<{ file: string; text: string }> {
  const file = path.join(mkdtempSync(path.join(tmpdir(), "reflog-")), "ref-log.jsonl");
  await appendEntries(file, [{ ref: "refs/heads/main", oldId: Z, newId: A }], "alice");
  await appendEntries(file, [{ ref: "refs/heads/main", oldId: A, newId: B }], "bob");
  await appendEntries(
    file,
    [
      { ref: "refs/heads/main", oldId: B, newId: C },
      { ref: "refs/tags/v1", oldId: Z, newId: B },
    ],
    "alice",
  );
  return { file, text: readFileSync(file, "utf8") };
}

function lines(text: string): RefLogEntry[] {
  return text
    .trim()
    .split("\n")
    .map((line) => JSON.parse(line) as RefLogEntry);
}

function serialise(entries: RefLogEntry[]): string {
  return `${entries.map((e) => JSON.stringify(e)).join("\n")}\n`;
}

describe("ref log chain", () => {
  it("replays an untouched log to the final ref state", async () => {
    const { text } = await sampleLog();
    const result = verifyChain(text);
    if (!result.ok) throw new Error(result.error);
    expect(result.entries.map((e) => e.seq)).toEqual([0, 1, 2, 3]);
    expect(Object.fromEntries(result.refs)).toEqual({
      "refs/heads/main": C,
      "refs/tags/v1": B,
    });
    expect(result.head).toBe(result.entries[3]?.hash ?? "");
  });

  it("detects an edited entry", async () => {
    const entries = lines((await sampleLog()).text);
    const edited = entries.map((e) => (e.seq === 1 ? { ...e, pusher: "mallory" } : e));
    const result = verifyChain(serialise(edited));
    expect(result.ok).toBe(false);
    if (!result.ok) expect(result.error).toContain("entry 1");
  });

  it("detects a dropped entry and a reordered pair", async () => {
    const entries = lines((await sampleLog()).text);
    expect(verifyChain(serialise(entries.filter((e) => e.seq !== 1))).ok).toBe(false);
    const swapped = [entries[0], entries[2], entries[1], entries[3]] as RefLogEntry[];
    expect(verifyChain(serialise(swapped)).ok).toBe(false);
  });

  it("detects a ref moving from a state the log never recorded", async () => {
    const entries = lines((await sampleLog()).text);
    // Re-hash consistently so only the ref-state replay can catch it.
    const forged: RefLogEntry[] = [];
    let prev = "0".repeat(64);
    for (const entry of entries) {
      const body = { ...entry, prev, old: entry.seq === 2 ? A : entry.old };
      const { hash: _drop, ...unhashed } = body;
      const hash = entryHash(unhashed);
      forged.push({ ...unhashed, hash });
      prev = hash;
    }
    const result = verifyChain(serialise(forged));
    expect(result.ok).toBe(false);
    if (!result.ok) expect(result.error).toContain("never had");
  });

  it("catches a consistent rewrite only through a pin, which is what pins are for", async () => {
    const original = verifyChain((await sampleLog()).text);
    if (!original.ok) throw new Error("sample broken");
    const pin = { seq: 1, hash: original.entries[1]?.hash ?? "" };

    const rewritten: RefLogEntry[] = [];
    let prev = "0".repeat(64);
    for (const entry of original.entries) {
      const { hash: _drop, ...body } = {
        ...entry,
        prev,
        pusher: entry.seq === 1 ? "x" : entry.pusher,
      };
      const hash = entryHash(body);
      rewritten.push({ ...body, hash });
      prev = hash;
    }
    const result = verifyChain(serialise(rewritten));
    expect(result.ok).toBe(true);
    if (result.ok) expect(extendsPin(result.entries, pin)).toBe(false);
    expect(extendsPin(original.entries, pin)).toBe(true);
    expect(extendsPin(original.entries, { seq: 9, hash: pin.hash })).toBe(false);
  });

  it("refuses to append to a log that is already broken", async () => {
    const { file, text } = await sampleLog();
    await Bun.write(file, text.replace('"bob"', '"eve"'));
    await expect(
      appendEntries(file, [{ ref: "refs/heads/main", oldId: C, newId: A }], "alice"),
    ).rejects.toThrow(/already broken/);
  });

  it("gives concurrent appends distinct sequence numbers", async () => {
    const file = path.join(mkdtempSync(path.join(tmpdir(), "reflog-")), "ref-log.jsonl");
    await Promise.all(
      Array.from({ length: 8 }, (_, i) =>
        appendEntries(file, [{ ref: `refs/heads/b${i}`, oldId: Z, newId: A }], `u${i}`),
      ),
    );
    const result = verifyChain(readFileSync(file, "utf8"));
    expect(result.ok).toBe(true);
    if (result.ok) expect(result.entries.length).toBe(8);
  });
});
