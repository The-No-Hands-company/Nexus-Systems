import { createHmac, randomUUID } from "node:crypto";
import type { ServiceTokenPayload, ValidateTokenResult } from "./types";

type Header = {
  alg: "HS256";
  typ: "JWT";
};

const encoder = new TextEncoder();

function base64UrlEncode(value: string): string {
  return Buffer.from(encoder.encode(value)).toString("base64url");
}

function base64UrlDecode(value: string): string {
  return Buffer.from(value, "base64url").toString("utf8");
}

function signingSecret(): string {
  return process.env.NEXUS_AUTH_TOKEN_SECRET || "nexus-auth-dev-secret";
}

function sign(value: string): string {
  return createHmac("sha256", signingSecret()).update(value).digest("base64url");
}

export function issueServiceToken(input: {
  serviceId: string;
  audience?: string;
  scopes?: string[];
  expiresInSeconds?: number;
}): { token: string; payload: ServiceTokenPayload } {
  const now = Math.floor(Date.now() / 1000);
  const expiresInSeconds = Math.max(60, input.expiresInSeconds ?? 3600);

  const payload: ServiceTokenPayload = {
    iss: "nexus-auth",
    sub: input.serviceId,
    aud: input.audience || "nexus-internal",
    scopes: input.scopes?.length ? input.scopes : ["service:read"],
    iat: now,
    exp: now + expiresInSeconds,
    jti: randomUUID(),
  };

  const header: Header = { alg: "HS256", typ: "JWT" };
  const encodedHeader = base64UrlEncode(JSON.stringify(header));
  const encodedPayload = base64UrlEncode(JSON.stringify(payload));
  const signingInput = `${encodedHeader}.${encodedPayload}`;
  const signature = sign(signingInput);

  return {
    token: `${signingInput}.${signature}`,
    payload,
  };
}

export function validateServiceToken(token: string, expectedAudience?: string): ValidateTokenResult {
  const parts = token.split(".");
  if (parts.length !== 3) {
    return { valid: false, reason: "invalid-token-format" };
  }

  const [encodedHeader, encodedPayload, providedSignature] = parts;
  const signingInput = `${encodedHeader}.${encodedPayload}`;
  const expectedSignature = sign(signingInput);

  if (providedSignature !== expectedSignature) {
    return { valid: false, reason: "invalid-signature" };
  }

  let parsedPayload: ServiceTokenPayload;
  try {
    parsedPayload = JSON.parse(base64UrlDecode(encodedPayload)) as ServiceTokenPayload;
  } catch {
    return { valid: false, reason: "invalid-payload" };
  }

  const now = Math.floor(Date.now() / 1000);
  if (parsedPayload.exp <= now) {
    return { valid: false, reason: "token-expired" };
  }

  if (expectedAudience && parsedPayload.aud !== expectedAudience) {
    return { valid: false, reason: "audience-mismatch" };
  }

  return { valid: true, payload: parsedPayload };
}
