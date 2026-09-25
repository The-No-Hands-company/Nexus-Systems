import { describe, expect, it } from "bun:test";
import { rankBetween } from "../src/rank";
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
