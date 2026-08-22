import { randomUUID } from "node:crypto";

/**
 * A real shell, attached to a pseudo-terminal.
 *
 * ## What this is
 *
 * This runs an actual login shell on this host, as the user this service runs
 * as. Not a command allow-list, not a sandbox: anything reachable over SSH by
 * that user is reachable here. That is the deliberate choice recorded in
 * docs/TERMINAL-SECURITY.md — read it before changing who can reach this.
 *
 * ## Why `script` rather than node-pty
 *
 * Interactive programs need a pseudo-terminal, not a pipe. Without one there is
 * no job control, no resize, no colour, and anything using readline or curses
 * (vim, top, less, even bash's own line editing) misbehaves or hangs.
 *
 * node-pty is the usual answer and needs native compilation. `script -qfc` is
 * in util-linux, is already on this machine, and allocates a genuine pty — a
 * shell started this way reports a real /dev/pts device. No native dependency,
 * no build step, one fewer thing to break on upgrade.
 *
 * The cost is that resize is not plumbed through: `script` has no channel for
 * TIOCSWINSZ. COLUMNS and LINES are set at spawn from the client's initial
 * size, so a shell starts correctly sized and does not follow later changes.
 */

export type PtySession = {
  id: string;
  /** Auth subject that owns this session. Sessions are never shared. */
  subject: string;
  proc: ReturnType<typeof Bun.spawn>;
  startedAt: number;
  lastActivityAt: number;
  closing: boolean;
  finished: Promise<number>;
  write(data: string): void;
  kill(): Promise<void>;
};

/** Hard ceiling on concurrent shells, so a loop cannot exhaust the host. */
const MAX_SESSIONS = 8;

/** A shell left open forever is a shell someone forgot they opened. */
const IDLE_TIMEOUT_MS = 30 * 60 * 1000;

const sessions = new Map<string, PtySession>();

export function sessionCount(): number {
  return sessions.size;
}

export function getSession(id: string): PtySession | undefined {
  return sessions.get(id);
}

/**
 * Start a shell for `subject`.
 *
 * `onData` receives raw terminal bytes as they arrive; `onExit` fires once.
 * Returns null when the ceiling is reached, which the caller reports rather
 * than queueing — a user waiting on an invisible queue has no idea why nothing
 * happened.
 */
export function spawnShell(opts: {
  subject: string;
  cols: number;
  rows: number;
  onData: (chunk: string) => void;
  onExit: (id: string, code: number) => void | Promise<void>;
  now?: () => number;
}): PtySession | null {
  if (sessions.size >= MAX_SESSIONS) return null;

  const id = randomUUID();
  const shell = process.env.SHELL || "/bin/bash";

  const proc = Bun.spawn(["script", "-qfc", shell, "/dev/null"], {
    stdin: "pipe",
    stdout: "pipe",
    stderr: "pipe",
    env: {
      ...process.env,
      TERM: "xterm-256color",
      // script cannot forward a resize, so the size is fixed at spawn.
      COLUMNS: String(Math.max(20, Math.min(500, opts.cols || 80))),
      LINES: String(Math.max(5, Math.min(200, opts.rows || 24))),
    },
  });

  const now = opts.now ?? Date.now;
  const startedAt = now();
  let killPromise: Promise<void> | null = null;

  const session: PtySession = {
    id,
    subject: opts.subject,
    proc,
    startedAt,
    lastActivityAt: startedAt,
    closing: false,
    finished: Promise.resolve(0),
    write(data) {
      if (data.length > 0) session.lastActivityAt = now();
      try {
        proc.stdin?.write(data);
        proc.stdin?.flush?.();
      } catch {
        // Process exit can race the final browser frame. The session's exit
        // path closes the socket; a stale frame must not crash the service.
      }
    },
    kill() {
      if (killPromise) return killPromise;
      session.closing = true;
      try { proc.stdin?.end(); } catch { /* already gone */ }
      try { proc.kill(); } catch { /* already gone */ }

      // A shell that ignores TERM must not make service shutdown hang forever.
      const forceTimer = setTimeout(() => {
        try { proc.kill(9); } catch { /* already gone */ }
      }, 500);
      forceTimer.unref?.();
      killPromise = session.finished.then(() => undefined).finally(() => clearTimeout(forceTimer));
      return killPromise;
    },
  };

  session.finished = proc.exited.then(async (code) => {
    const exitCode = typeof code === "number" ? code : 0;
    try {
      await opts.onExit(id, exitCode);
    } finally {
      sessions.delete(id);
    }
    return exitCode;
  });

  sessions.set(id, session);

  const pump = async (stream: ReadableStream<Uint8Array> | null) => {
    if (!stream) return;
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      const decoded = decoder.decode(chunk);
      if (decoded.length > 0) session.lastActivityAt = now();
      opts.onData(decoded);
    }
  };
  void pump(proc.stdout as ReadableStream<Uint8Array>);
  void pump(proc.stderr as ReadableStream<Uint8Array>);

  return session;
}

/**
 * Reap idle shells.
 *
 * Called on a timer by the server. Without this, every abandoned browser tab
 * leaves a live shell on the host until reboot, and the MAX_SESSIONS ceiling
 * eventually refuses new ones for no visible reason.
 */
export async function reapIdle(now = Date.now()): Promise<number> {
  const idle = [...sessions.values()].filter(
    (session) => !session.closing && now - session.lastActivityAt > IDLE_TIMEOUT_MS,
  );
  await Promise.all(idle.map((session) => session.kill()));
  return idle.length;
}

/** Stop everything. Used on shutdown so no shell outlives the service. */
export async function killAll(): Promise<void> {
  await Promise.all([...sessions.values()].map((session) => session.kill()));
}
