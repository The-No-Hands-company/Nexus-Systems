/**
 * tokenAuthMiddleware
 *
 * If the request already has a session user (set by authMiddleware), this is
 * a no-op. Otherwise it checks for a `Authorization: Bearer fh_<token>`
 * header and validates it against the api_tokens table.
 *
 * On success it populates req.user exactly like session-based middleware,
 * AND sets req.tokenScopes to the token's allowed scopes.
 *
 * Scope enforcement helpers:
 *   requireScope("deploy")  — use as route middleware
 *   req.hasScope("write")   — inline check
 *
 * Scopes: read | write | deploy | admin
 * Default for existing tokens: "read,write,deploy"
 */
import { type Request, type Response, type NextFunction } from "express";
import { db, apiTokensTable, usersTable } from "@workspace/db";
import { eq, and, isNull } from "drizzle-orm";
import { hashToken } from "../routes/tokens";
import { AppError } from "../lib/errors";
import logger from "../lib/logger";

export type TokenScope = "read" | "write" | "deploy" | "admin";

// Augment Express request
declare global {
  namespace Express {
    interface Request {
      tokenScopes?: Set<TokenScope>;
      hasScope?: (scope: TokenScope) => boolean;
    }
  }
}

export async function tokenAuthMiddleware(
  req: Request,
  _res: Response,
  next: NextFunction,
): Promise<void> {
  if (req.isAuthenticated?.()) { next(); return; }

  const authHeader = req.headers["authorization"];
  if (!authHeader?.startsWith("Bearer fh_")) { next(); return; }

  const plaintext = authHeader.slice(7);
  const tokenHash = hashToken(plaintext);

  try {
    const now = new Date();

    const [row] = await db
      .select({
        id:        apiTokensTable.id,
        userId:    apiTokensTable.userId,
        expiresAt: apiTokensTable.expiresAt,
        scopes:    apiTokensTable.scopes,
      })
      .from(apiTokensTable)
      .where(and(eq(apiTokensTable.tokenHash, tokenHash), isNull(apiTokensTable.revokedAt)));

    if (!row) { next(); return; }
    if (row.expiresAt && row.expiresAt < now) { next(); return; }

    // Parse scopes
    const scopeSet = new Set<TokenScope>(
      (row.scopes ?? "read,write,deploy").split(",").map(s => s.trim()) as TokenScope[]
    );

    // Update lastUsedAt fire-and-forget
    db.update(apiTokensTable).set({ lastUsedAt: now }).where(eq(apiTokensTable.id, row.id)).catch(() => {});

    const [user] = await db
      .select({
        id:              usersTable.id,
        email:           usersTable.email,
        firstName:       usersTable.firstName,
        lastName:        usersTable.lastName,
        profileImageUrl: usersTable.profileImageUrl,
      })
      .from(usersTable)
      .where(eq(usersTable.id, row.userId));

    if (user) {
      req.user = user;
      req.tokenScopes = scopeSet;
      req.hasScope    = (scope: TokenScope) => scopeSet.has(scope);
    }
  } catch (err) {
    logger.warn({ err }, "Token auth error");
  }

  next();
}

/**
 * Route middleware that enforces a required scope.
 * Session-based requests (no token) are always allowed through —
 * scopes only restrict API token access.
 */
export function requireScope(scope: TokenScope) {
  return (req: Request, _res: Response, next: NextFunction): void => {
    // Session auth — no scope restriction
    if (!req.tokenScopes) { next(); return; }
    if (!req.tokenScopes.has(scope)) {
      next(AppError.forbidden(`This token does not have '${scope}' scope. Re-generate with the required permissions.`));
      return;
    }
    next();
  };
}
