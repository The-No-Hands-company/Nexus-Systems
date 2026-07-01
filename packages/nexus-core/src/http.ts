// ─── HTTP Helpers ─────────────────────────────────────────────────────────
// Response builders, body parsers, security headers — shared by all Nexus apps.

import type { IncomingMessage, ServerResponse } from "node:http";
import type { SecurityHeaders } from "./types";

// ── Security Headers ──────────────────────────────────────────────────────

export const SECURITY_HEADERS: SecurityHeaders = {
  "x-content-type-options": "nosniff",
  "x-frame-options": "DENY",
  "cache-control": "no-store",
};

// ── JSON Response ─────────────────────────────────────────────────────────

export function sendJson(
  res: ServerResponse,
  status: number,
  payload: unknown,
  requestId: string,
): void {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "content-length": Buffer.byteLength(body),
    "x-request-id": requestId,
    ...SECURITY_HEADERS,
  });
  res.end(body);
}

// ── JSON Error Response (convenience) ─────────────────────────────────────

export function sendError(
  res: ServerResponse,
  status: number,
  error: string,
  code: string,
  requestId: string,
): void {
  sendJson(res, status, { error, code, requestId }, requestId);
}

// ── Body Parsers ──────────────────────────────────────────────────────────

export async function readJsonBody(req: IncomingMessage): Promise<unknown> {
  const chunks: Buffer[] = [];
  for await (const chunk of req) chunks.push(chunk as Buffer);
  const raw = Buffer.concat(chunks).toString("utf8");
  return raw ? JSON.parse(raw) : {};
}

export async function readRawBody(req: IncomingMessage): Promise<Buffer> {
  const chunks: Buffer[] = [];
  for await (const chunk of req) chunks.push(chunk as Buffer);
  return Buffer.concat(chunks);
}

// ── Multipart Parsing ─────────────────────────────────────────────────────

export function parseMultipartBoundary(contentType: string): string | null {
  const match = contentType.match(/boundary=(?:"([^"]+)"|([^;]+))/);
  if (!match) return null;
  return (match[1] ?? match[2] ?? "").trim();
}

export interface MultipartPart {
  headers: Record<string, string>;
  body: Buffer;
}

export function parseMultipart(buffer: Buffer, boundary: string): MultipartPart[] {
  const parts: MultipartPart[] = [];
  const delimiter = Buffer.from(`--${boundary}`);
  const crlf = Buffer.from("\r\n\r\n");
  let pos = 0;

  while (pos < buffer.length) {
    const delimIdx = buffer.indexOf(delimiter, pos);
    if (delimIdx < 0) break;
    pos = delimIdx + delimiter.length;

    // Check for closing delimiter
    if (pos + 2 <= buffer.length && buffer[pos] === 45 && buffer[pos + 1] === 45) break;

    // Skip leading CRLF
    if (pos + 1 < buffer.length && buffer[pos] === 13 && buffer[pos + 1] === 10) pos += 2;

    const headerEnd = buffer.indexOf(crlf, pos);
    if (headerEnd < 0) break;
    const headerBlock = buffer.slice(pos, headerEnd).toString("utf8");
    pos = headerEnd + crlf.length;

    const headers: Record<string, string> = {};
    for (const line of headerBlock.split("\r\n")) {
      const colonIdx = line.indexOf(":");
      if (colonIdx > 0) {
        const key = line.slice(0, colonIdx).trim().toLowerCase();
        const value = line.slice(colonIdx + 1).trim();
        headers[key] = value;
      }
    }

    const nextDelim = buffer.indexOf(delimiter, pos);
    const bodyEnd = nextDelim >= 0 ? nextDelim : buffer.length;
    let body = buffer.slice(pos, bodyEnd);

    // Strip trailing CRLF
    if (body.length >= 2 && body[body.length - 2] === 13 && body[body.length - 1] === 10) {
      body = body.slice(0, -2);
    }

    parts.push({ headers, body });
    pos = bodyEnd;
  }

  return parts;
}

// ── Content Type Detection ────────────────────────────────────────────────

const CONTENT_TYPES: Record<string, string> = {
  ".txt": "text/plain",
  ".html": "text/html",
  ".css": "text/css",
  ".js": "application/javascript",
  ".json": "application/json",
  ".xml": "application/xml",
  ".pdf": "application/pdf",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".svg": "image/svg+xml",
  ".webp": "image/webp",
  ".mp3": "audio/mpeg",
  ".mp4": "video/mp4",
  ".wav": "audio/wav",
  ".zip": "application/zip",
  ".tar": "application/x-tar",
  ".gz": "application/gzip",
  ".md": "text/markdown",
  ".yaml": "application/x-yaml",
  ".yml": "application/x-yaml",
  ".csv": "text/csv",
  ".wasm": "application/wasm",
  ".ttf": "font/ttf",
  ".woff2": "font/woff2",
};

export function detectContentType(fileName: string, fallback = "application/octet-stream"): string {
  const ext = fileName.slice(fileName.lastIndexOf(".")).toLowerCase();
  return CONTENT_TYPES[ext] ?? fallback;
}
