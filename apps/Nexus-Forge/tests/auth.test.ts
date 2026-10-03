import { beforeEach, describe, expect, it } from "bun:test";
import { authorize } from "../src/backend/auth/access";
import { issueToken, principalFromRequest, userForToken } from "../src/backend/auth/tokens";
import { ForgeDB } from "../src/backend/storage/db";

let db: ForgeDB;
beforeEach(() => {
  db = new ForgeDB(":memory:");
});

function basic(user: string, password: string): Request {
  const value = Buffer.from(`${user}:${password}`).toString("base64");
  return new Request("http://forge/", { headers: { authorization: `Basic ${value}` } });
}

describe("access tokens", () => {
  it("resolves a live token to its user and stores only a hash", () => {
    const alice = db.addUser("alice");
    const token = issueToken(db, alice.id);
    expect(userForToken(db, token)?.username).toBe("alice");
    const stored = db.db.query("SELECT token_hash FROM tokens").all() as { token_hash: string }[];
    expect(stored.map((row) => row.token_hash)).not.toContain(token);
  });

  it("rejects expired, revoked, unknown and malformed tokens", () => {
    const alice = db.addUser("alice");
    const now = 1_000_000;
    const token = issueToken(db, alice.id, 1000, now);
    expect(userForToken(db, token, now + 999)?.id).toBe(alice.id);
    expect(userForToken(db, token, now + 1000)).toBeNull();

    const other = issueToken(db, alice.id);
    db.revokeTokensFor(alice.id);
    expect(userForToken(db, other)).toBeNull();

    expect(userForToken(db, "nxf_not-a-real-token")).toBeNull();
    expect(userForToken(db, "")).toBeNull();
  });

  it("refuses tokens that never expire or outlive the maximum", () => {
    const alice = db.addUser("alice");
    expect(() => issueToken(db, alice.id, 0)).toThrow();
    expect(() => issueToken(db, alice.id, Number.POSITIVE_INFINITY)).toThrow();
    expect(() => issueToken(db, alice.id, 365 * 24 * 3600 * 1000)).toThrow();
  });

  it("reads the token from Bearer and from the Basic password, ignoring the Basic username", () => {
    const alice = db.addUser("alice");
    db.addUser("mallory");
    const token = issueToken(db, alice.id);
    const bearer = new Request("http://forge/", {
      headers: { authorization: `Bearer ${token}` },
    });
    expect(principalFromRequest(db, bearer)?.username).toBe("alice");
    expect(principalFromRequest(db, basic("mallory", token))?.username).toBe("alice");
    expect(principalFromRequest(db, basic("alice", "wrong"))).toBeNull();
    expect(principalFromRequest(db, new Request("http://forge/"))).toBeNull();
    const garbage = new Request("http://forge/", { headers: { authorization: "Basic ***" } });
    expect(principalFromRequest(db, garbage)).toBeNull();
  });
});

describe("authorize", () => {
  it("is default-deny: no grant means no access to a private repository", () => {
    const owner = db.addUser("owner");
    const stranger = db.addUser("stranger");
    const repo = db.addRepository("secret", "private", owner.id, null);
    for (const action of ["read", "write", "admin"] as const) {
      expect(authorize(db, null, repo, action)).toBe(false);
      expect(authorize(db, stranger, repo, action)).toBe(false);
      expect(authorize(db, owner, repo, action)).toBe(true);
    }
  });

  it("lets anyone read a public repository but nobody write it without a grant", () => {
    const owner = db.addUser("owner");
    const stranger = db.addUser("stranger");
    const repo = db.addRepository("open", "public", owner.id, null);
    expect(authorize(db, null, repo, "read")).toBe(true);
    expect(authorize(db, null, repo, "write")).toBe(false);
    expect(authorize(db, stranger, repo, "read")).toBe(true);
    expect(authorize(db, stranger, repo, "write")).toBe(false);
  });

  it("orders levels read < write < admin", () => {
    const owner = db.addUser("owner");
    const reader = db.addUser("reader");
    const writer = db.addUser("writer");
    const repo = db.addRepository("r", "private", owner.id, null);
    db.setGrant(repo.id, reader.id, "read");
    db.setGrant(repo.id, writer.id, "write");
    expect([
      authorize(db, reader, repo, "read"),
      authorize(db, reader, repo, "write"),
      authorize(db, writer, repo, "write"),
      authorize(db, writer, repo, "admin"),
    ]).toEqual([true, false, true, false]);
  });
});

describe("ForgeDB", () => {
  it("refuses a database whose schema is newer than the code", () => {
    const path = `${Bun.env.TMPDIR ?? "/tmp"}/forge-schema-${crypto.randomUUID()}.db`;
    const first = new ForgeDB(path);
    first.db.exec("PRAGMA user_version = 99");
    first.close();
    expect(() => new ForgeDB(path)).toThrow(/newer/);
  });

  it("lists only repositories the caller can read", () => {
    const owner = db.addUser("owner");
    const stranger = db.addUser("stranger");
    db.addRepository("open", "public", owner.id, null);
    db.addRepository("secret", "private", owner.id, null);
    const names = (id: number | null) => db.listReadableRepositories(id).map((r) => r.name);
    expect(names(null)).toEqual(["open"]);
    expect(names(stranger.id)).toEqual(["open"]);
    expect(names(owner.id).sort()).toEqual(["open", "secret"]);
  });
});
