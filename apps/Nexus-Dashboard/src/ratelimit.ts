/**
 * Rate limiting for Dashboard API endpoints.
 *
 * Uses a token bucket algorithm with in-memory storage. Suitable for single-instance
 * deployments; for multi-instance deployments, use a distributed store (Redis).
 */

type Bucket = {
  tokens: number;
  lastRefill: number;
};

const buckets = new Map<string, Bucket>();

interface RateLimitConfig {
  maxTokens: number;
  refillRatePerSecond: number;
  keyPrefix?: string;
}

const DEFAULT_CONFIG: RateLimitConfig = {
  maxTokens: 100,
  refillRatePerSecond: 10,
};

function getBucket(key: string): Bucket {
  const now = Date.now();
  const bucket = buckets.get(key);
  if (!bucket) {
    const newBucket: Bucket = { tokens: DEFAULT_CONFIG.maxTokens, lastRefill: Date.now() };
    buckets.set(key, newBucket);
    return newBucket;
  }

  const elapsed = (now - bucket.lastRefill) / 1000;
  const refillAmount = elapsed * DEFAULT_CONFIG.refillRatePerSecond;
  bucket.tokens = Math.min(DEFAULT_CONFIG.maxTokens, bucket.tokens + refillAmount);
  bucket.lastRefill = now;
  return bucket;
}

function getClientKey(req: Request, prefix: string = "default"): string {
  const ip = req.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ||
    req.headers.get("x-real-ip") ||
    "unknown";
  return `${prefix}:${ip}`;
}

export interface RateLimitResult {
  allowed: boolean;
  remaining: number;
  resetMs: number;
}

export function checkRateLimit(
  req: Request,
  config?: Partial<RateLimitConfig>
): RateLimitResult {
  const cfg = { ...DEFAULT_CONFIG, ...config };
  const key = getClientKey(req, cfg.keyPrefix);
  const bucket = getBucket(key);

  if (bucket.tokens >= 1) {
    bucket.tokens -= 1;
    return {
      allowed: true,
      remaining: Math.floor(bucket.tokens),
      resetMs: Math.ceil((DEFAULT_CONFIG.maxTokens - bucket.tokens) / DEFAULT_CONFIG.refillRatePerSecond * 1000),
    };
  }

  const resetMs = Math.ceil((1 - bucket.tokens) / DEFAULT_CONFIG.refillRatePerSecond * 1000);
  return {
    allowed: false,
    remaining: 0,
    resetMs,
  };
}

export function createRateLimitResponse(result: RateLimitResult): Response {
  const headers = new Headers({
    "X-RateLimit-Limit": String(DEFAULT_CONFIG.maxTokens),
    "X-RateLimit-Remaining": String(result.remaining),
    "X-RateLimit-Reset": String(Math.ceil(result.resetMs / 1000)),
    "Retry-After": String(Math.ceil(result.resetMs / 1000)),
  });

  return new Response(JSON.stringify({ error: "rate_limit_exceeded" }), {
    status: 429,
    headers,
  });
}

export function rateLimitMiddleware(
  config?: Partial<RateLimitConfig>
) {
  return async (req: Request, next: () => Promise<Response>): Promise<Response> => {
    const result = checkRateLimit(req, config);
    if (!result.allowed) {
      return createRateLimitResponse(result);
    }
    const response = await next();
    // Add rate limit headers to successful responses too
    response.headers.set("X-RateLimit-Limit", String(DEFAULT_CONFIG.maxTokens));
    response.headers.set("X-RateLimit-Remaining", String(result.remaining));
    response.headers.set("X-RateLimit-Reset", String(Math.ceil(result.resetMs / 1000)));
    return response;
  };
}

/** Clears every bucket. Tests use this; a restart achieves the same thing. */
export function resetRateLimits(): void {
  buckets.clear();
}