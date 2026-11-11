import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-edge-tests-"));

process.env.NEXUS_EDGE_STATE_PATH = join(dir, "edge-state.json");
