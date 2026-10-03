/**
 * pre-receive: refuses the whole push unless every ref update passes the
 * signed-push policy. Runs inside receive-pack, so git commands here see the
 * pushed objects in the quarantine directory git set up in our environment.
 */
import { readFile } from "node:fs/promises";
import path from "node:path";
import { type Git, checkPush, parseUpdates } from "../policy/verify";
import { TRUST_ROOT_FILE } from "../storage/repository";

const git: Git = async (args, config = {}) => {
  const flags = Object.entries(config).flatMap(([key, value]) => ["-c", `${key}=${value}`]);
  const proc = Bun.spawn(["git", ...flags, ...args], { stdout: "pipe", stderr: "pipe" });
  const [stdout, stderr, code] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  return { code, stdout, stderr };
};

async function main(): Promise<number> {
  const meta = process.env.NEXUS_FORGE_META;
  if (!meta) {
    console.error("forge: hook environment missing; refusing push");
    return 1;
  }
  const trustRoot = await readFile(path.join(meta, TRUST_ROOT_FILE), "utf8").catch(() => null);
  const updates = parseUpdates(await new Response(Bun.stdin.stream()).text());
  const errors = await checkPush(git, updates, trustRoot);
  for (const error of errors) console.error(`forge: ${error}`);
  return errors.length === 0 ? 0 : 1;
}

main().then(
  (code) => process.exit(code),
  (error) => {
    console.error(`forge: policy check crashed (${(error as Error).message}); refusing push`);
    process.exit(1);
  },
);
