import http from 'node:http';
import https from 'node:https';
import type { IncomingMessage } from 'node:http';
import type { HttpProbeResult } from './types.js';

const PROBE_TIMEOUT_MS = 2500;
/** Max bytes of response body to buffer for title extraction */
const BODY_LIMIT = 8192;
/** Max redirects to follow */
const MAX_REDIRECTS = 2;

// Ports that are almost certainly TLS — try https:// first
const TLS_PORTS = new Set([443, 8443, 9443]);

// ─────────────────────────────────────────────────────────────────────────────
// Header helpers
// ─────────────────────────────────────────────────────────────────────────────

function header(res: IncomingMessage, name: string): string | null {
  const v = res.headers[name.toLowerCase()];
  if (!v) return null;
  return Array.isArray(v) ? v[0] : v;
}

function extractTitle(html: string): string | null {
  const m = html.match(/<title[^>]*>([^<]{1,200})<\/title>/i);
  return m ? m[1].trim().replace(/\s+/g, ' ') : null;
}

/**
 * Extract the `frame-ancestors` directive value from a Content-Security-Policy header.
 * Returns the raw directive value (e.g. `'self'`, `*`, `https://cloud.example.com`), or null.
 */
function extractFrameAncestors(csp: string | null): string | null {
  if (!csp) return null;
  const m = csp.match(/(?:^|;)\s*frame-ancestors\s+([^;]+)/i);
  return m ? m[1].trim() : null;
}

/**
 * Determine whether a service can be embedded in an <iframe> by another origin.
 *
 * Rules:
 * - `X-Frame-Options: DENY`        → not embeddable
 * - `X-Frame-Options: SAMEORIGIN`  → not embeddable (from a different origin)
 * - CSP `frame-ancestors 'none'`   → not embeddable
 * - CSP `frame-ancestors 'self'`   → not embeddable from external origin
 * - CSP `frame-ancestors` with no wildcard and no explicit external origin → not embeddable
 * - Otherwise → embeddable
 */
export function isEmbeddable(
  xFrameOptions: string | null,
  frameAncestors: string | null,
): boolean {
  if (xFrameOptions) {
    const xfo = xFrameOptions.toUpperCase().trim();
    if (xfo === 'DENY' || xfo === 'SAMEORIGIN') return false;
  }

  if (frameAncestors) {
    const fa = frameAncestors.toLowerCase().trim();
    if (fa === "'none'" || fa === 'none') return false;
    if (fa === "'self'" || fa === 'self') return false;
    // No wildcard → restricted (even if specific origins are listed, we can't guarantee ours)
    if (!fa.includes('*')) return false;
  }

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Core probe function
// ─────────────────────────────────────────────────────────────────────────────

function probeSingleUrl(
  url: string,
  tls: boolean,
  redirectsLeft = MAX_REDIRECTS,
): Promise<HttpProbeResult> {
  return new Promise((resolve) => {
    const start = Date.now();
    const elapsed = () => Date.now() - start;
    const client = tls ? https : http;

    const req = client.get(
      url,
      {
        timeout: PROBE_TIMEOUT_MS,
        rejectUnauthorized: false,
        headers: {
          'User-Agent': 'nexus-porter/1.0 (+port-intelligence)',
          Accept: 'text/html,application/json,*/*',
        },
      },
      (res) => {
        // Follow redirects
        if (
          redirectsLeft > 0 &&
          res.statusCode !== undefined &&
          res.statusCode >= 300 &&
          res.statusCode < 400 &&
          res.headers.location
        ) {
          res.resume(); // drain
          const location = res.headers.location;
          const nextTls = location.startsWith('https://');
          probeSingleUrl(location, nextTls, redirectsLeft - 1).then(resolve);
          return;
        }

        const chunks: Buffer[] = [];
        let size = 0;

        res.on('data', (chunk: Buffer) => {
          size += chunk.length;
          if (size <= BODY_LIMIT) chunks.push(chunk);
          else res.destroy(); // we have enough
        });

        res.on('end', () => {
          const body = Buffer.concat(chunks).toString('utf8', 0, BODY_LIMIT);
          const csp           = header(res, 'content-security-policy');
          const xFrameOptions = header(res, 'x-frame-options');
          const frameAncestors = extractFrameAncestors(csp);
          const contentTypeFull = header(res, 'content-type');
          const contentType     = contentTypeFull?.split(';')[0]?.trim();

          const title = contentType?.includes('html') ? extractTitle(body) : null;

          resolve({
            url,
            reachable: true,
            statusCode: res.statusCode,
            serverHeader: header(res, 'server') ?? undefined,
            title: title ?? undefined,
            contentType,
            xFrameOptions,
            frameAncestors,
            embeddable: isEmbeddable(xFrameOptions, frameAncestors),
            tls,
            responseTimeMs: elapsed(),
          });
        });

        res.on('error', (err: Error) => {
          resolve({
            url, reachable: false, embeddable: false, tls,
            error: err.message, responseTimeMs: elapsed(),
          });
        });
      },
    );

    req.on('timeout', () => {
      req.destroy();
      resolve({
        url, reachable: false, embeddable: false, tls,
        error: 'timeout', responseTimeMs: PROBE_TIMEOUT_MS,
      });
    });

    req.on('error', (err: NodeJS.ErrnoException) => {
      // ECONNREFUSED → not an HTTP listener on this port
      resolve({
        url, reachable: false, embeddable: false, tls,
        error: err.code ?? err.message, responseTimeMs: elapsed(),
      });
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Probe a local port for HTTP/HTTPS service.
 *
 * - TLS ports (443, 8443, 9443) try https:// first.
 * - All other ports try http:// first; if unreachable, fallback to https://.
 * - Returns null when the port is definitely not an HTTP listener (ECONNREFUSED).
 */
export async function probePort(port: number): Promise<HttpProbeResult | null> {
  const tls = TLS_PORTS.has(port);
  const scheme = tls ? 'https' : 'http';
  const url = `${scheme}://127.0.0.1:${port}/`;

  const result = await probeSingleUrl(url, tls);

  if (!result.reachable && !tls) {
    // Try HTTPS in case it's an atypical TLS port
    const tlsResult = await probeSingleUrl(`https://127.0.0.1:${port}/`, true);
    if (tlsResult.reachable) return tlsResult;
  }

  // Connection refused → not an HTTP listener; return the result so callers know
  if (!result.reachable && result.error === 'ECONNREFUSED') return result;

  return result;
}
