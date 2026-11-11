import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-tunnel-tests-"));

process.env.NEXUS_TUNNEL_STATE_PATH = join(dir, "tunnel-state.json");
