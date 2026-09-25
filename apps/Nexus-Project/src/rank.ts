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
