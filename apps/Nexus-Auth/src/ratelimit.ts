/**
 * Fixed-window failure counting for endpoints where the request body IS the
 * secret — claim codes, recovery codes and invite codes are all 128-bit values
 * that a determined attacker would otherwise be free to guess at line rate.
 *
 * Only failures count. A caller supplying a correct code is not consuming
 * budget, so a legitimate user is never locked out by their own success.
 *
 * In-memory and per-process: restarting Nexus-Auth clears the counters, and a
 * second node would count separately. Acceptable while there is one node —
 * revisit when the gate is federated.
 */

const DEFAULT_MAX = 5;
const DEFAULT_WINDOW_MS = 15 * 60_000;

type Entry = { count: number; firstFailureAt: number };

const failures = new Map<string, Entry>();

function compositeKey(bucket: string, key: string): string {
  return `${bucket}:${key}`;
}

export function checkRateLimit(
  bucket: string,
  key: string,
  opts?: { max?: number; windowMs?: number },
): { allowed: true } | { allowed: false; retryAfterMs: number } {
  const max = opts?.max ?? DEFAULT_MAX;
  const windowMs = opts?.windowMs ?? DEFAULT_WINDOW_MS;

  const entry = failures.get(compositeKey(bucket, key));
  if (!entry) return { allowed: true };

  const elapsed = Date.now() - entry.firstFailureAt;
  if (elapsed >= windowMs) {
    failures.delete(compositeKey(bucket, key));
    return { allowed: true };
  }
  if (entry.count < max) return { allowed: true };

  return { allowed: false, retryAfterMs: windowMs - elapsed };
}

export function recordFailure(bucket: string, key: string): void {
  const composite = compositeKey(bucket, key);
  const entry = failures.get(composite);
  if (entry) {
    entry.count += 1;
    failures.set(composite, entry);
  } else {
    failures.set(composite, { count: 1, firstFailureAt: Date.now() });
  }
}

export function clearFailures(bucket: string, key: string): void {
  failures.delete(compositeKey(bucket, key));
}

export function resetRateLimits(): void {
  failures.clear();
}
