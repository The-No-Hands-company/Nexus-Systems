import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

/**
 * Points every persistent store at a throwaway directory before any test module
 * is loaded. Registered via `[test] preload` in bunfig.toml.
 *
 * This has to be a preload rather than per-file setup. Each store module
 * resolves its path once, at import time, and the modules import each other —
 * users.ts pulls in recovery.ts — so the first test file to be loaded fixes the
 * path for every file after it. Setting the variables inside a test file only
 * works if that file happens to sort first, and a file that forgot to set them
 * (tests/auth.test.ts did) would run against the real store and call
 * clearUsers() on it.
 *
 * That is not hypothetical: it wrote data/auth-recovery.json into the live data
 * directory before this preload existed.
 */
const dir = mkdtempSync(join(tmpdir(), "nexus-auth-tests-"));

process.env.NEXUS_AUTH_USER_STORE_PATH = join(dir, "users.json");
process.env.NEXUS_AUTH_RECOVERY_STORE_PATH = join(dir, "recovery.json");
process.env.NEXUS_AUTH_INVITE_STORE_PATH = join(dir, "invites.json");
