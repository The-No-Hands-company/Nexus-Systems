/**
 * Admin authorization middleware.
 *
 * Checks whether the authenticated user has admin/operator privileges.
 * Two ways to be an admin:
 *   1. users.is_admin = 1 in the database (set manually or via admin promotion)
 *   2. User ID listed in ADMIN_USER_IDS env var (comma-separated, for bootstrap)
 *
 * Usage:
 *   router.get("/admin/overview", requireAdmin, asyncHandler(async (req, res) => { ... }))
 */

import { type Request, type Response, type NextFunction } from "express";
import { db, usersTable } from "@workspace/db";
import { eq } from "drizzle-orm";
import { AppError } from "../lib/errors";
import logger from "../lib/logger";

/** Parse the ADMIN_USER_IDS env var into a Set for O(1) lookup */
function getAdminUserIds(): Set<string> {
  const raw = process.env.ADMIN_USER_IDS ?? "";
  return new Set(raw.split(",").map((id) => id.trim()).filter(Boolean));
}

/**
 * Check if a user ID has admin privileges.
 * Checks env var first (no DB query), then DB flag.
 */
export async function isAdminUser(userId: string): Promise<boolean> {
  // Fast path: env var override (useful for bootstrapping first admin)
  if (getAdminUserIds().has(userId)) return true;

  // DB flag check
  const [user] = await db
    .select({ isAdmin: usersTable.isAdmin })
    .from(usersTable)
    .where(eq(usersTable.id, userId));

  return (user?.isAdmin ?? 0) === 1;
}

/**
 * Express middleware that rejects non-admin requests with 403.
 * Must be placed after authMiddleware.
 */
export async function requireAdmin(
  req: Request,
  res: Response,
  next: NextFunction,
): Promise<void> {
  if (!req.isAuthenticated()) {
    next(AppError.unauthorized());
    return;
  }

  try {
    const admin = await isAdminUser(req.user.id);
    if (!admin) {
      logger.warn(
        { userId: req.user.id, path: req.path },
        "[rbac] Non-admin attempted to access admin endpoint",
      );
      next(AppError.forbidden("Admin access required"));
      return;
    }
    next();
  } catch (err) {
    next(err);
  }
}
