/**
 * post-receive: appends the accepted ref updates to the repository's
 * hash-chained ref log. The refs have already moved when this runs, so a
 * failure here cannot undo the push; it leaves the log behind the refs,
 * which `forge verify` reports as a mismatch rather than hiding.
 */
import path from "node:path";
import { parseUpdates } from "../policy/verify";
import { appendEntries } from "../reflog/chain";
import { REF_LOG_FILE } from "../storage/repository";

async function main(): Promise<number> {
  const meta = process.env.NEXUS_FORGE_META;
  if (!meta) {
    console.error("forge: hook environment missing; ref log NOT updated");
    return 1;
  }
  const updates = parseUpdates(await new Response(Bun.stdin.stream()).text());
  await appendEntries(path.join(meta, REF_LOG_FILE), updates, process.env.NEXUS_FORGE_PUSHER ?? "");
  return 0;
}

main().then(
  (code) => process.exit(code),
  (error) => {
    console.error(`forge: ref log NOT updated (${(error as Error).message})`);
    process.exit(1);
  },
);
