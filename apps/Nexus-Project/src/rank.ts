const DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";
const BASE = DIGITS.length;

/**
 * A string strictly between `before` and `after` in lexicographic order.
 * `null` is an open end. Results never end in "0", so there is always room to
 * insert before any rank this returns.
 */
export function rankBetween(before: string | null, after: string | null): string {
  if (before !== null && after !== null && before >= after) {
    throw new RangeError(`rank bounds out of order: ${before} >= ${after}`);
  }
  let upper = after;
  let result = "";
  for (let i = 0; ; i++) {
    const low = before !== null && i < before.length ? DIGITS.indexOf(before[i] as string) : 0;
    const high = upper !== null && i < upper.length ? DIGITS.indexOf(upper[i] as string) : BASE;
    if (high - low > 1) return result + DIGITS[Math.floor((low + high) / 2)];
    result += DIGITS[low];
    // Once this prefix is below the upper bound's prefix, later digits of the
    // upper bound no longer constrain anything.
    if (low < high) upper = null;
  }
}

const WIDTH = 4;
const digit = (c: string) => DIGITS.indexOf(c);

/**
 * A rank strictly after `rank` for appending at the open end. Fixed-width
 * increment (at least four positions), so a long run of appends stays short
 * instead of bisecting toward the end one character per few calls. A carry
 * resets lower positions to "1", never "0", so the result never ends in "0".
 */
export function rankAfter(rank: string): string {
  const padded = rank.padEnd(WIDTH, "0");
  const chars = [...padded];
  for (let i = chars.length - 1; i >= 0; i--) {
    const value = digit(chars[i] as string);
    if (value < BASE - 1) {
      chars[i] = DIGITS[value + 1] as string;
      for (let j = i + 1; j < chars.length; j++) chars[j] = "1";
      return chars.join("");
    }
  }
  return `${rank}i`; // every position is "z"
}

/**
 * A rank strictly before `rank` for inserting at the open start: the
 * fixed-width decrement counterpart of rankAfter. The last position never
 * drops below "1" (a borrow sets lower positions to "z"), so the result never
 * ends in "0"; only an underflow (every position at its minimum) grows it by
 * one character.
 */
export function rankBefore(rank: string): string {
  // Padding with "0" is safe: the decrement then borrows through the padding
  // into the original characters, so the result stays below `rank`.
  const chars = [...rank.padEnd(WIDTH, "0")];
  for (let i = chars.length - 1; i >= 0; i--) {
    const floor = i === chars.length - 1 ? 1 : 0;
    const value = digit(chars[i] as string);
    if (value > floor) {
      chars[i] = DIGITS[value - 1] as string;
      for (let j = i + 1; j < chars.length; j++) chars[j] = "z";
      return chars.join("");
    }
  }
  // Underflow: "0…01" (or all zeros). Extend below the last position.
  const result = `${chars.slice(0, -1).join("")}0z`;
  if (result >= rank) throw new RangeError(`no rank sorts before ${rank}`);
  return result;
}
