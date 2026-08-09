import { createSign, createVerify, randomUUID, timingSafeEqual } from "node:crypto";
import { privateKey, publicKey, signingAlgorithm, signingKid } from "./keys";
import type { ServiceTokenPayload, ValidateTokenResult } from "./types";

type Header = {
  alg: "RS256";
  typ: "JWT";
  kid: string;
};

const encoder = new TextEncoder();

function base64UrlEncode(value: string): string {
  return Buffer.from(encoder.encode(value)).toString("base64url");
}

function base64UrlDecode(value: string): string {
  return Buffer.from(value, "base64url").toString("utf8");
}

/*
 * RS256, not HMAC. The old scheme signed with NEXUS_AUTH_TOKEN_SECRET and the
 * JWKS endpoint published that same secret, so any unauthenticated caller could
 * fetch it and mint tokens. Under an asymmetric scheme the private key never
 * leaves this process and JWKS carries only the public half — verifiers can
 * check a token without gaining the ability to issue one, which is what lets an
 * app be self-hosted without being trusted.
 */
function sign(value: string): string {
  return createSign("RSA-SHA256").update(value).end().sign(privateKey, "base64url");
}

function verify(value: string, signature: string): boolean {
  try {
    return createVerify("RSA-SHA256")
      .update(value)
      .end()
      .verify(publicKey, Buffer.from(signature, "base64url"));
  } catch {
    return false;
  }
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
    role: "service",
    iat: now,
    exp: now + expiresInSeconds,
    jti: randomUUID(),
  };

  const header: Header = { alg: signingAlgorithm as "RS256", typ: "JWT", kid: signingKid };
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

  // Reject the algorithm outright rather than trusting the header. Accepting
  // whatever `alg` a token declares is the classic JWT confusion attack — an
  // attacker downgrades to "none", or to HMAC using the public key as the
  // shared secret.
  try {
    const header = JSON.parse(base64UrlDecode(encodedHeader)) as Partial<Header>;
    if (header.alg !== "RS256") {
      return { valid: false, reason: "invalid-signature" };
    }
  } catch {
    return { valid: false, reason: "invalid-token-format" };
  }

  if (!verify(signingInput, providedSignature)) {
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
