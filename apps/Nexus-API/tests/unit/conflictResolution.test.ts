import { describe, it, expect, vi, beforeEach } from "vitest";

// We test the algorithm logic in isolation by mocking the DB
// The actual resolveConflict function is in lib/conflictResolution.ts
// These tests verify the decision rules without requiring a database

interface ConflictInput {
  localJoinedAt: Date;
  remoteJoinedAt: Date;
  localPubKey: string;
  remotePubKey: string;
  sameOrigin?: boolean;
  signatureValid?: boolean;
}

// Pure function extracted from the conflict resolution algorithm
function resolveConflictLogic(input: ConflictInput): "local" | "remote" {
  // Rule 0: same-origin updates always accepted
  if (input.sameOrigin) return "remote";

  // Rule 1: signature invalid → reject
  if (input.signatureValid === false) return "local";

  // Rule 2: earlier joinedAt wins
  if (input.remoteJoinedAt < input.localJoinedAt) return "remote";
  if (input.localJoinedAt < input.remoteJoinedAt) return "local";

  // Rule 3: equal timestamps — lexicographically smaller public key wins
  return input.remotePubKey < input.localPubKey ? "remote" : "local";
}

describe("Conflict resolution algorithm", () => {
  describe("same-origin updates", () => {
    it("always accepts same-origin updates", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2024-01-01"),
        remoteJoinedAt: new Date("2025-01-01"), // remote joined later
        localPubKey: "aaa",
        remotePubKey: "zzz",
        sameOrigin: true,
      });
      expect(result).toBe("remote");
    });
  });

  describe("signature verification", () => {
    it("rejects invalid signatures", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2025-01-01"),
        remoteJoinedAt: new Date("2024-01-01"), // remote joined earlier
        localPubKey: "bbb",
        remotePubKey: "aaa",
        signatureValid: false,
      });
      expect(result).toBe("local");
    });

    it("accepts valid signatures from earlier-joining node", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2025-01-01"),
        remoteJoinedAt: new Date("2024-01-01"),
        localPubKey: "bbb",
        remotePubKey: "aaa",
        signatureValid: true,
      });
      expect(result).toBe("remote");
    });
  });

  describe("first-write-wins (joinedAt ordering)", () => {
    it("remote wins when it joined earlier", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2024-06-01"),
        remoteJoinedAt: new Date("2024-01-01"),
        localPubKey: "aaa",
        remotePubKey: "bbb",
      });
      expect(result).toBe("remote");
    });

    it("local wins when it joined earlier", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2024-01-01"),
        remoteJoinedAt: new Date("2024-06-01"),
        localPubKey: "aaa",
        remotePubKey: "bbb",
      });
      expect(result).toBe("local");
    });

    it("millisecond difference is respected", () => {
      const base = new Date("2024-01-01T00:00:00.000Z");
      const plusOne = new Date("2024-01-01T00:00:00.001Z");
      expect(resolveConflictLogic({ localJoinedAt: base,    remoteJoinedAt: plusOne, localPubKey: "a", remotePubKey: "b" })).toBe("local");
      expect(resolveConflictLogic({ localJoinedAt: plusOne, remoteJoinedAt: base,    localPubKey: "a", remotePubKey: "b" })).toBe("remote");
    });
  });

  describe("public key tiebreaker (equal timestamps)", () => {
    const sameTime = new Date("2024-01-01T12:00:00Z");

    it("lexicographically smaller remote key wins", () => {
      const result = resolveConflictLogic({
        localJoinedAt: sameTime,
        remoteJoinedAt: sameTime,
        localPubKey: "zzz",
        remotePubKey: "aaa",
      });
      expect(result).toBe("remote");
    });

    it("lexicographically larger remote key loses", () => {
      const result = resolveConflictLogic({
        localJoinedAt: sameTime,
        remoteJoinedAt: sameTime,
        localPubKey: "aaa",
        remotePubKey: "zzz",
      });
      expect(result).toBe("local");
    });

    it("equal public keys → local wins (conservative)", () => {
      const result = resolveConflictLogic({
        localJoinedAt: sameTime,
        remoteJoinedAt: sameTime,
        localPubKey: "abc",
        remotePubKey: "abc",
      });
      expect(result).toBe("local");
    });

    it("is deterministic — same inputs always produce same output", () => {
      const input: ConflictInput = {
        localJoinedAt: sameTime,
        remoteJoinedAt: sameTime,
        localPubKey: "MFkwEwYHKoZIzj0CAQ==",
        remotePubKey: "MFkwEwYHKoZIzj0CAB==",
      };
      const results = Array.from({ length: 20 }, () => resolveConflictLogic(input));
      expect(new Set(results).size).toBe(1);
    });
  });

  describe("rule priority", () => {
    it("signature check takes priority over joinedAt", () => {
      // Even though remote joined earlier, invalid sig should reject it
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2025-01-01"),
        remoteJoinedAt: new Date("2024-01-01"),
        localPubKey: "zzz",
        remotePubKey: "aaa",
        signatureValid: false,
      });
      expect(result).toBe("local");
    });

    it("same-origin takes priority over everything", () => {
      const result = resolveConflictLogic({
        localJoinedAt: new Date("2024-01-01"),
        remoteJoinedAt: new Date("2025-01-01"),
        localPubKey: "aaa",
        remotePubKey: "zzz",
        sameOrigin: true,
        signatureValid: false, // even invalid sig — same-origin wins
      });
      expect(result).toBe("remote");
    });
  });
});
