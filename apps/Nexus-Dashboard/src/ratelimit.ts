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

function getBucket(key: string, cfg: RateLimitConfig = DEFAULT_CONFIG): Bucket {
  const now = Date.now();
  const bucket = buckets.get(key);
  if (!bucket) {
    const newBucket: Bucket = { tokens: cfg.maxTokens, lastRefill: now };
    buckets.set(key, newBucket);
    return newBucket;
  }

  const elapsed = (now - bucket.lastRefill) / 1000;
  const refillAmount = elapsed * cfg.refillRatePerSecond;
  bucket.tokens = Math.min(cfg.maxTokens, bucket.tokens + refillAmount);
  bucket.lastRefill = now;
  return bucket;
}

function getClientKey(req: Request, prefix = "default"): string {
  // Keyed on the proxy's opaque client tag, session cookie, identity, subject, or connecting IP
  const cookie = req.headers.get("cookie") || "";
  const sessionMatch = cookie.match(/nexus_session=([^;]+)/);
  const clientTag = req.headers.get("x-nexus-client-tag");
  const connectingIp = req.headers.get("cf-connecting-ip");
  const identity = req.headers.get("x-nexus-identity");
  const subject = req.headers.get("x-nexus-subject");
  const forwarded = req.headers.get("x-forwarded-for")?.split(",")[0]?.trim();

  const tag =
    (clientTag && clientTag !== "unknown" ? clientTag : null) ||
    (sessionMatch ? `sess_${sessionMatch[1].slice(0, 16)}` : null) ||
    (identity ? `id_${identity.slice(0, 16)}` : null) ||
    (subject ? `sub_${subject}` : null) ||
    connectingIp ||
    forwarded ||
    "unknown";
  return `${prefix}:${tag}`;
}

export interface RateLimitResult {
  allowed: boolean;
  remaining: number;
  resetMs: number;
}

export function checkRateLimit(req: Request, config?: Partial<RateLimitConfig>): RateLimitResult {
  const cfg = { ...DEFAULT_CONFIG, ...config };
  const key = getClientKey(req, cfg.keyPrefix);
  const bucket = getBucket(key, cfg);

  if (bucket.tokens >= 1) {
    bucket.tokens -= 1;
    return {
      allowed: true,
      remaining: Math.floor(bucket.tokens),
      resetMs: Math.ceil(
        ((cfg.maxTokens - bucket.tokens) / cfg.refillRatePerSecond) * 1000,
      ),
    };
  }

  const resetMs = Math.ceil(((1 - bucket.tokens) / cfg.refillRatePerSecond) * 1000);
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

export function rateLimitMiddleware(config?: Partial<RateLimitConfig>) {
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
