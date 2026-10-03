/**
 * Signer lists use git's own `gpg.ssh.allowedSignersFile` format
 * (`principal [options] keytype base64key`, one per line), so the file a
 * developer already keeps for `git verify-commit` is the file the forge
 * enforces.
 */
const KEY_TYPES = new Set([
  "ssh-ed25519",
  "sk-ssh-ed25519@openssh.com",
  "ecdsa-sha2-nistp256",
  "ecdsa-sha2-nistp384",
  "ecdsa-sha2-nistp521",
  "sk-ecdsa-sha2-nistp256@openssh.com",
  "ssh-rsa",
]);

/** Number of signer entries, or an error describing the first bad line. */
export function parseAllowedSigners(text: string): { count: number } | { error: string } {
  let count = 0;
  const lines = text.split("\n");
  for (const [index, raw] of lines.entries()) {
    const line = raw.trim();
    if (line === "" || line.startsWith("#")) continue;
    const fields = line.split(/\s+/);
    const typeAt = fields.findIndex((field) => KEY_TYPES.has(field));
    const key = typeAt > 0 ? fields[typeAt + 1] : undefined;
    if (!key || !/^[A-Za-z0-9+/]+={0,2}$/.test(key)) {
      return { error: `line ${index + 1}: expected "principal [options] keytype key"` };
    }
    count++;
  }
  if (count === 0) return { error: "no signer entries" };
  return { count };
}

/** Signer lines without comments, blank lines or surrounding space, in file order. */
export function normaliseSigners(text: string): string {
  return text
    .split("\n")
    .map((line) => line.trim())
    .filter((line) => line !== "" && !line.startsWith("#"))
    .join("\n");
}

/**
 * A trust root's fingerprint, `sha256:<hex>` over its normalised signer
 * lines. Short enough to read out or paste into a chat, which is how a
 * client gets the trust root from somewhere other than the server.
 */
export function trustRootFingerprint(text: string): string {
  return `sha256:${new Bun.CryptoHasher("sha256").update(normaliseSigners(text)).digest("hex")}`;
}
