import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-guardian-tests-"));

process.env.NEXUS_GUARDIAN_STATE_PATH = join(dir, "guardian-state.json");
