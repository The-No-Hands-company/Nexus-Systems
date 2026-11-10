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
 * ## Session Persistence & Reattachment (tmux-style)
 *
 * Terminal shells support persistence: when a client disconnects (or reloads the
 * browser tab), the shell process remains alive in the background. When reconnecting
 * with the same sessionId, the client reattaches to the live shell and receives
 * the recent scrollback buffer. Detached sessions are reaped if unattached for >15m.
 */

export type PtySession = {
  id: string;
  /** Auth subject that owns this session. Sessions are never shared. */
  subject: string;
  proc: ReturnType<typeof Bun.spawn>;
  startedAt: number;
  lastActivityAt: number;
  detachedAt: number | null;
  cols: number;
  rows: number;
  closing: boolean;
  finished: Promise<number>;
  outputBuffer: string[];
  outputBytes: number;
  listeners: Set<(chunk: string) => void>;
  exitCallbacks: Set<(id: string, code: number) => void | Promise<void>>;
  attach(onData: (chunk: string) => void, onExit?: (id: string, code: number) => void | Promise<void>): void;
  detach(onData: (chunk: string) => void): void;
  write(data: string): void;
  kill(): Promise<void>;
};

/** Hard ceiling on concurrent shells, so a loop cannot exhaust the host. */
const MAX_SESSIONS = 8;

/** A shell left open forever is a shell someone forgot they opened. */
const IDLE_TIMEOUT_MS = 30 * 60 * 1000;

/** A detached shell without reconnect is reaped after 15 minutes. */
const DETACH_TIMEOUT_MS = 15 * 60 * 1000;

/** Ring buffer size for terminal scrollback on reattach (64KB). */
const MAX_HISTORY_BYTES = 64 * 1024;

const sessions = new Map<string, PtySession>();

export function sessionCount(): number {
  return sessions.size;
}

export function getSession(id: string): PtySession | undefined {
  return sessions.get(id);
}

export async function killSession(id: string): Promise<boolean> {
  const session = sessions.get(id);
  if (!session) return false;
  await session.kill();
  return true;
}

export function listActiveSessions(subject?: string): Array<{
  id: string;
  subject: string;
  startedAt: number;
  lastActivityAt: number;
  detached: boolean;
  cols: number;
  rows: number;
}> {
  return [...sessions.values()]
    .filter((s) => !s.closing && (!subject || s.subject === subject))
    .map((s) => ({
      id: s.id,
      subject: s.subject,
      startedAt: s.startedAt,
      lastActivityAt: s.lastActivityAt,
      detached: s.detachedAt !== null || s.listeners.size === 0,
      cols: s.cols,
      rows: s.rows,
    }));
}

/**
 * Start a shell for `subject`, or reattach to an existing session if `opts.sessionId` matches.
 *
 * `onData` receives raw terminal bytes as they arrive; `onExit` fires once.
 * Returns null when the ceiling is reached, which the caller reports rather
 * than queueing.
 */
export function spawnShell(opts: {
  subject: string;
  cols: number;
  rows: number;
  sessionId?: string;
  onData: (chunk: string) => void;
  onExit: (id: string, code: number) => void | Promise<void>;
  now?: () => number;
}): PtySession | null {
  const now = opts.now ?? Date.now;

  // ── Session Reattachment (tmux-style) ──────────────────────────────────
  if (opts.sessionId && sessions.has(opts.sessionId)) {
    const existing = sessions.get(opts.sessionId)!;
    if (existing.subject === opts.subject && !existing.closing) {
      existing.lastActivityAt = now();
      existing.detachedAt = null;
      existing.attach(opts.onData, opts.onExit);

      // Replay existing scrollback buffer so client restores screen
      const history = existing.outputBuffer.join("");
      if (history.length > 0) {
        opts.onData(history);
      }
      return existing;
    }
  }

  if (sessions.size >= MAX_SESSIONS) return null;

  const id = opts.sessionId || randomUUID();
  const shell = process.env.SHELL || "/bin/bash";
  const cols = Math.max(20, Math.min(500, opts.cols || 80));
  const rows = Math.max(5, Math.min(200, opts.rows || 24));

  const proc = Bun.spawn(["script", "-qfc", shell, "/dev/null"], {
    stdin: "pipe",
    stdout: "pipe",
    stderr: "pipe",
    env: {
      ...process.env,
      TERM: "xterm-256color",
      COLUMNS: String(cols),
      LINES: String(rows),
    },
  });

  const startedAt = now();
  let killPromise: Promise<void> | null = null;
  const listeners = new Set<(chunk: string) => void>([opts.onData]);
  const exitCallbacks = new Set<(id: string, code: number) => void | Promise<void>>([opts.onExit]);

  const session: PtySession = {
    id,
    subject: opts.subject,
    proc,
    startedAt,
    lastActivityAt: startedAt,
    detachedAt: null,
    cols,
    rows,
    closing: false,
    finished: Promise.resolve(0),
    outputBuffer: [],
    outputBytes: 0,
    listeners,
    exitCallbacks,
    attach(onData, onExit) {
      session.listeners.add(onData);
      if (onExit) session.exitCallbacks.add(onExit);
      session.detachedAt = null;
    },
    detach(onData) {
      session.listeners.delete(onData);
      if (session.listeners.size === 0) {
        session.detachedAt = now();
      }
    },
    write(data) {
      if (data.length > 0) session.lastActivityAt = now();
      try {
        proc.stdin?.write(data);
        proc.stdin?.flush?.();
      } catch {
        // Process exit can race the final browser frame.
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
      for (const cb of session.exitCallbacks) {
        try { await cb(id, exitCode); } catch { /* callback fault */ }
      }
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
      if (decoded.length > 0) {
        session.lastActivityAt = now();
        session.outputBuffer.push(decoded);
        session.outputBytes += decoded.length;
        while (session.outputBytes > MAX_HISTORY_BYTES && session.outputBuffer.length > 1) {
          const dropped = session.outputBuffer.shift()!;
          session.outputBytes -= dropped.length;
        }
      }
      for (const listener of session.listeners) {
        try { listener(decoded); } catch { /* listener closed */ }
      }
    }
  };
  void pump(proc.stdout as ReadableStream<Uint8Array>);
  void pump(proc.stderr as ReadableStream<Uint8Array>);

  return session;
}

/**
 * Reap idle or abandoned detached shells.
 *
 * Called on a timer by the server.
 */
export async function reapIdle(now = Date.now()): Promise<number> {
  const idle = [...sessions.values()].filter(
    (session) =>
      !session.closing &&
      (now - session.lastActivityAt > IDLE_TIMEOUT_MS ||
        (session.detachedAt !== null && now - session.detachedAt > DETACH_TIMEOUT_MS)),
  );
  await Promise.all(idle.map((session) => session.kill()));
  return idle.length;
}

/** Stop everything. Used on shutdown so no shell outlives the service. */
export async function killAll(): Promise<void> {
  await Promise.all([...sessions.values()].map((session) => session.kill()));
}
