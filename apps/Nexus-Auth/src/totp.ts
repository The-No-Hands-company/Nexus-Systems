import { createHmac, randomBytes, timingSafeEqual } from "node:crypto";

const BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

export function encodeBase32(buffer: Uint8Array): string {
  let bits = 0;
  let value = 0;
  let output = "";

  for (let i = 0; i < buffer.length; i++) {
    value = (value << 8) | buffer[i];
    bits += 8;

    while (bits >= 5) {
      output += BASE32_ALPHABET[(value >>> (bits - 5)) & 31];
      bits -= 5;
    }
  }

  if (bits > 0) {
    output += BASE32_ALPHABET[(value << (5 - bits)) & 31];
  }

  return output;
}

export function decodeBase32(input: string): Uint8Array {
  const cleaned = input.toUpperCase().replace(/=+$/, "").replace(/\s+/g, "");
  let bits = 0;
  let value = 0;
  const result: number[] = [];

  for (let i = 0; i < cleaned.length; i++) {
    const idx = BASE32_ALPHABET.indexOf(cleaned[i]);
    if (idx === -1) {
      throw new Error(`Invalid base32 character: ${cleaned[i]}`);
    }

    value = (value << 5) | idx;
    bits += 5;

    if (bits >= 8) {
      result.push((value >>> (bits - 8)) & 255);
      bits -= 8;
    }
  }

  return new Uint8Array(result);
}

export function generateTotpSecret(bytes = 20): string {
  return encodeBase32(randomBytes(bytes));
}

export function generateTotpCode(secretBase32: string, counter?: number): string {
  const step = 30;
  const count = counter !== undefined ? counter : Math.floor(Date.now() / 1000 / step);

  const secretBytes = decodeBase32(secretBase32);
  const counterBuffer = Buffer.alloc(8);
  // Big-endian 64-bit integer
  counterBuffer.writeBigUInt64BE(BigInt(count));

  const hmac = createHmac("sha1", secretBytes).update(counterBuffer).digest();
  const offset = hmac[hmac.length - 1] & 0x0f;
  const codeInt =
    ((hmac[offset] & 0x7f) << 24) |
    ((hmac[offset + 1] & 0xff) << 16) |
    ((hmac[offset + 2] & 0xff) << 8) |
    (hmac[offset + 3] & 0xff);

  const otp = codeInt % 1_000_000;
  return otp.toString().padStart(6, "0");
}

export function verifyTotpCode(secretBase32: string, token: string, window = 1): boolean {
  if (!token || typeof token !== "string") return false;
  const cleanToken = token.trim().replace(/\s+/g, "");
  if (!/^\d{6}$/.test(cleanToken)) return false;

  const step = 30;
  const currentCount = Math.floor(Date.now() / 1000 / step);

  for (let offset = -window; offset <= window; offset++) {
    const expected = generateTotpCode(secretBase32, currentCount + offset);
    if (expected.length === cleanToken.length) {
      const match = timingSafeEqual(Buffer.from(expected), Buffer.from(cleanToken));
      if (match) return true;
    }
  }

  return false;
}

export function buildTotpUri(username: string, secretBase32: string, issuer = "Nexus"): string {
  const label = encodeURIComponent(`${issuer}:${username}`);
  const encIssuer = encodeURIComponent(issuer);
  return `otpauth://totp/${label}?secret=${secretBase32}&issuer=${encIssuer}&algorithm=SHA1&digits=6&period=30`;
}
