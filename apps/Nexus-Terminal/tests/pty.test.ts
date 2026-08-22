import { afterEach, describe, expect, it } from "bun:test";
import { getSession, killAll, reapIdle, sessionCount, spawnShell } from "../src/pty";

const IDLE_TIMEOUT_MS = 30 * 60 * 1_000;

type ActivitySession = NonNullable<ReturnType<typeof spawnShell>> & {
  lastActivityAt: number;
};

function shell(
  onData: (chunk: string) => void = () => {},
  now: () => number = Date.now,
): ActivitySession {
  const session = spawnShell({
    subject: "test-operator",
    cols: 80,
    rows: 24,
    onData,
    onExit: () => {},
    now,
  });
  if (!session) throw new Error("expected a PTY session");
  return session as ActivitySession;
}

async function waitUntil(predicate: () => boolean, label: string): Promise<void> {
  const deadline = Date.now() + 3_000;
  while (Date.now() < deadline) {
    if (predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error(`timed out waiting for ${label}`);
}

afterEach(async () => {
  await killAll();
});

describe("PTY session lifecycle", () => {
  it("reaps a genuinely idle shell but preserves an old shell with recent activity", async () => {
    let clock = 0;
    let idleOutput = "";
    let activeOutput = "";
    const idle = shell((chunk) => { idleOutput += chunk; }, () => clock);
    const active = shell((chunk) => { activeOutput += chunk; }, () => clock);

    // Drain each initial prompt while the fake clock is still at zero. This
    // prevents asynchronous shell startup output from making the idle peer
    // look active after the clock advances.
    await waitUntil(() => idleOutput.length > 0 && activeOutput.length > 0, "initial PTY output");
    clock = IDLE_TIMEOUT_MS + 10_000;
    active.write(" ");

    // Both sessions are older than the timeout. Only recent activity should
    // distinguish them; using startedAt would incorrectly terminate both.
    expect(await reapIdle(clock)).toBe(1);
    expect(getSession(idle.id)).toBeUndefined();
    expect(getSession(active.id)).toBe(active);
  });

  it("updates activity on operator input and PTY output", async () => {
    let clock = 10;
    let transcript = "";
    const session = shell((chunk) => {
      transcript += chunk;
    }, () => clock);

    session.lastActivityAt = 0;
    clock = 20;
    session.write("sleep 0.05; printf 'activity-marker\\n'\r");
    expect(session.lastActivityAt).toBe(20);

    // Reset after input so only output can satisfy the second assertion.
    session.lastActivityAt = 0;
    const bytesBeforeOutput = transcript.length;
    clock = 30;
    await waitUntil(
      () => transcript.length > bytesBeforeOutput,
      "PTY output after operator input",
    );
    expect(session.lastActivityAt).toBe(30);
  });

  it("enforces the eight-shell ceiling", async () => {
    const sessions = Array.from({ length: 8 }, () => shell());

    expect(sessionCount()).toBe(8);
    expect(
      spawnShell({
        subject: "ninth-operator",
        cols: 80,
        rows: 24,
        onData: () => {},
        onExit: () => {},
      }),
    ).toBeNull();

    await Promise.all(sessions.map((session) => session.kill()));
  });

  it("awaits process exit and invokes onExit once across repeated kills", async () => {
    let exits = 0;
    const session = spawnShell({
      subject: "test-operator",
      cols: 80,
      rows: 24,
      onData: () => {},
      onExit: () => {
        exits += 1;
      },
    });
    if (!session) throw new Error("expected a PTY session");

    await Promise.all([session.kill(), session.kill()]);

    expect(exits).toBe(1);
    expect(getSession(session.id)).toBeUndefined();
  });

  it("killAll resolves only after every shell has exited", async () => {
    let exits = 0;
    for (let index = 0; index < 2; index += 1) {
      const session = spawnShell({
        subject: `test-operator-${index}`,
        cols: 80,
        rows: 24,
        onData: () => {},
        onExit: () => {
          exits += 1;
        },
      });
      if (!session) throw new Error("expected a PTY session");
    }

    await killAll();

    expect(exits).toBe(2);
    expect(sessionCount()).toBe(0);
  });
});
