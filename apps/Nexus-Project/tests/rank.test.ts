import { describe, expect, it } from "bun:test";
import { rankAfter, rankBefore, rankBetween } from "../src/rank";
import { mulberry32, randomInt } from "./support/random";

describe("rankBetween", () => {
  it("starts in the middle of the space", () => {
    expect(rankBetween(null, null)).toBe("i");
  });

  it("always lands strictly between its bounds", () => {
    const next = mulberry32(7);
    const ranks: string[] = [rankBetween(null, null)];
    // Insert 2,000 times at random positions, including both ends, and check
    // the defining property every time rather than a final count.
    for (let i = 0; i < 2000; i++) {
      const at = randomInt(next, 0, ranks.length);
      const before = at === 0 ? null : (ranks[at - 1] as string);
      const after = at === ranks.length ? null : (ranks[at] as string);
      const rank = rankBetween(before, after);
      if (before !== null) expect(rank > before).toBe(true);
      if (after !== null) expect(rank < after).toBe(true);
      ranks.splice(at, 0, rank);
    }
    expect(new Set(ranks).size).toBe(ranks.length);
    expect([...ranks].sort()).toEqual(ranks);
  });

  it("never produces a rank ending in the lowest digit, so there is always room before it", () => {
    let rank = rankBetween(null, null);
    for (let i = 0; i < 200; i++) {
      rank = rankBetween(null, rank);
      expect(rank.endsWith("0")).toBe(false);
    }
  });

  it("refuses bounds in the wrong order", () => {
    expect(() => rankBetween("m", "c")).toThrow(RangeError);
    expect(() => rankBetween("m", "m")).toThrow(RangeError);
  });
});

describe("rankAfter and rankBefore", () => {
  function walk(step: (rank: string) => string): string[] {
    const ranks = ["i"];
    for (let i = 0; i < 10_000; i++) ranks.push(step(ranks[ranks.length - 1] as string));
    return ranks;
  }

  for (const [name, step, increasing] of [
    ["rankAfter", rankAfter, true],
    ["rankBefore", rankBefore, false],
  ] as const) {
    it(`${name}: 10,000 sequential calls stay strictly ordered, short and never end in "0"`, () => {
      const ranks = walk(step);
      let longest = 0;
      for (let i = 1; i < ranks.length; i++) {
        const previous = ranks[i - 1] as string;
        const rank = ranks[i] as string;
        expect(increasing ? rank > previous : rank < previous).toBe(true);
        expect(rank.endsWith("0")).toBe(false);
        longest = Math.max(longest, rank.length);
      }
      expect(longest).toBeLessThanOrEqual(6);
    });

    it(`${name}: rankBetween still fits between every adjacent pair`, () => {
      const ranks = walk(step);
      for (let i = 1; i < ranks.length; i++) {
        const [low, high] = increasing
          ? [ranks[i - 1] as string, ranks[i] as string]
          : [ranks[i] as string, ranks[i - 1] as string];
        const middle = rankBetween(low, high);
        expect(middle > low && middle < high).toBe(true);
      }
    });
  }

  it("carries into '1', never '0', and extends only when every position is at its limit", () => {
    expect(rankAfter("i")).toBe("i001");
    expect(rankAfter("i00z")).toBe("i011");
    expect(rankAfter("zzzz")).toBe("zzzzi");
    expect(rankBefore("i")).toBe("hzzz");
    expect(rankBefore("i011")).toBe("i00z");
    expect(rankBefore("0001")).toBe("0000z");
    expect(() => rankBefore("0")).toThrow(RangeError);
  });
});
