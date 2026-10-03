/**
 * The environment every git process the forge starts runs in.
 *
 * It is built from nothing rather than filtered from `process.env`: an
 * inherited `GIT_DIR`, `GIT_SSH_COMMAND`, `GIT_CONFIG_*` or a user's
 * ~/.gitconfig with an `alias`, `core.fsmonitor` or `core.sshCommand` would
 * otherwise run inside the forge's trust boundary. `config` entries are
 * passed through GIT_CONFIG_COUNT so they cannot be overridden by anything a
 * repository's own config says.
 */
export function gitEnv(
  config: Record<string, string> = {},
  extra: Record<string, string> = {},
): Record<string, string> {
  const env: Record<string, string> = {
    PATH: process.env.PATH ?? "/usr/local/bin:/usr/bin:/bin",
    HOME: "/nonexistent",
    LANG: "C",
    LC_ALL: "C",
    GIT_CONFIG_NOSYSTEM: "1",
    GIT_CONFIG_GLOBAL: "/dev/null",
    GIT_TERMINAL_PROMPT: "0",
  };
  const entries = Object.entries(config);
  env.GIT_CONFIG_COUNT = String(entries.length);
  entries.forEach(([key, value], index) => {
    env[`GIT_CONFIG_KEY_${index}`] = key;
    env[`GIT_CONFIG_VALUE_${index}`] = value;
  });
  return { ...env, ...extra };
}

export interface GitResult {
  code: number;
  stdout: string;
  stderr: string;
}

/** Run git with a fixed argument vector; no shell is ever involved. */
export async function runGit(
  args: string[],
  options: { cwd?: string; config?: Record<string, string>; stdin?: string } = {},
): Promise<GitResult> {
  const proc = Bun.spawn(["git", ...args], {
    ...(options.cwd ? { cwd: options.cwd } : {}),
    env: gitEnv(options.config),
    stdin: options.stdin !== undefined ? new TextEncoder().encode(options.stdin) : "ignore",
    stdout: "pipe",
    stderr: "pipe",
  });
  const [stdout, stderr, code] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  return { code, stdout, stderr };
}
