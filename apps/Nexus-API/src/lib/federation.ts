import crypto from "crypto";

export interface KeyPair {
  publicKey: string;
  privateKey: string;
}

export function generateKeyPair(): KeyPair {
  const { publicKey, privateKey } = crypto.generateKeyPairSync("ed25519", {
    publicKeyEncoding: { type: "spki", format: "pem" },
    privateKeyEncoding: { type: "pkcs8", format: "pem" },
  });
  return { publicKey, privateKey };
}

export function signMessage(privateKeyPem: string, message: string): string {
  const privateKey = crypto.createPrivateKey(privateKeyPem);
  return crypto.sign(null, Buffer.from(message), privateKey).toString("base64");
}

export function verifySignature(publicKeyPem: string, message: string, signature: string): boolean {
  try {
    const publicKey = crypto.createPublicKey(publicKeyPem);
    return crypto.verify(null, Buffer.from(message), publicKey, Buffer.from(signature, "base64"));
  } catch {
    return false;
  }
}

export function createFederationChallenge(): string {
  return crypto.randomBytes(32).toString("hex");
}

export function stripPemHeaders(pem: string): string {
  return pem
    .replace(/-----BEGIN [^-]+-----/g, "")
    .replace(/-----END [^-]+-----/g, "")
    .replace(/\s+/g, "");
}

export function addSpkiHeader(base64Key: string): string {
  const lines = base64Key.match(/.{1,64}/g)?.join("\n") ?? base64Key;
  return `-----BEGIN PUBLIC KEY-----\n${lines}\n-----END PUBLIC KEY-----\n`;
}
