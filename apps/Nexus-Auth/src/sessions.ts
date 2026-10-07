import type { Session } from "./types";
import { randomBytes, randomUUID } from "node:crypto";

const sessions = new Map<string, Session>();
const tokenIndex = new Map<string, string>();

function generateSessionId(): string {
  return `ses-${randomUUID()}`;
}

export function createSession(input: {
  userId: string;
  expiresInHours?: number;
}): Session {
  const now = new Date().toISOString();
  const expiresAt = new Date(
    Date.now() + (input.expiresInHours || 24) * 60 * 60 * 1000,
  ).toISOString();

  const session: Session = {
    id: generateSessionId(),
    userId: input.userId,
    token: `nxs_${randomUUID()}`,
    deviceId: `dev_${randomBytes(16).toString("base64url")}`,
    createdAt: now,
    expiresAt,
    revoked: false,
  };

  sessions.set(session.id, session);
  tokenIndex.set(session.token, session.id);
  return session;
}

export function validateSession(token: string): Session | undefined {
  const sessionId = tokenIndex.get(token);
  if (!sessionId) return undefined;

  const session = sessions.get(sessionId);
  if (!session || session.revoked) return undefined;
  if (new Date(session.expiresAt) < new Date()) return undefined;

  return session;
}

export function getSession(sessionId: string): Session | undefined {
  return sessions.get(sessionId);
}

export function listSessions(userId?: string): Session[] {
  const all = Array.from(sessions.values());
  return userId ? all.filter((s) => s.userId === userId) : all;
}

export function revokeSession(sessionId: string): boolean {
  const session = sessions.get(sessionId);
  if (!session || session.revoked) return false;
  session.revoked = true;
  tokenIndex.delete(session.token);
  return true;
}

export function revokeAllUserSessions(userId: string): number {
  let count = 0;
  for (const session of sessions.values()) {
    if (session.userId === userId && !session.revoked) {
      session.revoked = true;
      tokenIndex.delete(session.token);
      count++;
    }
  }
  return count;
}

export function clearSessions(): void {
  sessions.clear();
  tokenIndex.clear();
}
