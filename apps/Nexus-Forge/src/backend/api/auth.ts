import { env } from "bun";
import jwt from "jsonwebtoken";
import type { Elysia } from "elysia";

const JWT_SECRET = env.JWT_SECRET || "nexus-forge-secret";

export interface ForgeUser {
  id: number;
  username: string;
  role: "admin" | "maintainer" | "developer" | "viewer";
}

export function generateToken(user: ForgeUser) {
  return jwt.sign({ sub: user.id, username: user.username, role: user.role }, JWT_SECRET, {
    expiresIn: "8h",
  });
}

export function verifyToken(token: string): ForgeUser | null {
  try {
    const payload = jwt.verify(token, JWT_SECRET) as Record<string, unknown>;
    return {
      id: Number(payload.sub ?? 0),
      username: String(payload.username ?? "anonymous"),
      role: payload.role === "admin" ? "admin" : "viewer",
    };
  } catch {
    return null;
  }
}

export function authRoutes(app: Elysia) {
  app.post("/api/auth/login", async ({ body }) => {
    const payload = body as { username?: string; password?: string };
    // TODO: Replace with real user store / password validation.
    const user: ForgeUser = {
      id: 1,
      username: payload.username || "admin",
      role: "admin",
    };
    return { token: generateToken(user), user };
  });

  app.get("/api/auth/status", async ({ headers }) => {
    const authHeader = headers["authorization"];
    if (!authHeader?.startsWith("Bearer ")) {
      return { authenticated: false };
    }
    const token = authHeader.slice(7);
    const user = verifyToken(token);
    return { authenticated: !!user, user };
  });
}
