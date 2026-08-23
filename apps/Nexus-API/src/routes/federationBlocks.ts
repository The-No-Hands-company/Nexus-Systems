/**
 * Federation blocklist routes.
 *
 * Operators can block specific peer nodes from federating with this node.
 * Blocked nodes cannot handshake, ping, sync, or appear in bootstrap.
 *
 * Routes:
 *   GET    /api/federation/blocks           — list blocked nodes (admin)
 *   POST   /api/federation/blocks           — add a block (admin)
 *   DELETE /api/federation/blocks/:domain   — remove a block (admin)
 *   GET    /api/federation/blocks/check     — check if a domain is blocked (public — for peers)
 */

import { Router, type IRouter, type Request, type Response } from "express";
import { z } from "zod/v4";
import { db, federationBlocksTable, nodesTable } from "@workspace/db";
import { eq } from "drizzle-orm";
import { asyncHandler, AppError } from "../lib/errors";
import { requireAdmin } from "../middleware/requireAdmin";
import { writeLimiter } from "../middleware/rateLimiter";
import logger from "../lib/logger";

const router: IRouter = Router();

// In-memory Set for O(1) block checks on every incoming federation request.
// Loaded at startup and kept in sync with DB mutations.
export const blockedDomains = new Set<string>();

/** Load all blocked domains into the in-memory set at startup */
export async function loadBlocklist(): Promise<void> {
  try {
    const rows = await db.select({ nodeDomain: federationBlocksTable.nodeDomain }).from(federationBlocksTable);
    blockedDomains.clear();
    for (const row of rows) blockedDomains.add(row.nodeDomain.toLowerCase());
    logger.info({ count: blockedDomains.size }, "[blocklist] Loaded federation blocklist");
  } catch (err) {
    logger.warn({ err }, "[blocklist] Failed to load blocklist — continuing without it");
  }
}

/** Returns true if the given domain is on the blocklist */
export function isBlocked(domain: string): boolean {
  return blockedDomains.has(domain.toLowerCase());
}

// ── GET /api/federation/blocks ────────────────────────────────────────────────

router.get("/federation/blocks", requireAdmin, asyncHandler(async (req: Request, res: Response) => {
  const blocks = await db.select().from(federationBlocksTable).orderBy(federationBlocksTable.createdAt);
  res.json({ blocks, total: blocks.length });
}));

// ── POST /api/federation/blocks ───────────────────────────────────────────────

router.post("/federation/blocks", requireAdmin, writeLimiter, asyncHandler(async (req: Request, res: Response) => {
  const { nodeDomain, reason } = z.object({
    nodeDomain: z.string().min(1).max(253).toLowerCase(),
    reason:     z.string().max(500).optional(),
  }).parse(req.body);

  // Check if already blocked
  const [existing] = await db.select({ id: federationBlocksTable.id }).from(federationBlocksTable)
    .where(eq(federationBlocksTable.nodeDomain, nodeDomain));
  if (existing) throw AppError.conflict(`${nodeDomain} is already blocked`);

  const [block] = await db.insert(federationBlocksTable).values({
    nodeDomain,
    reason:    reason ?? null,
    blockedBy: req.user?.id ?? null,
  }).returning();

  // Update in-memory set immediately
  blockedDomains.add(nodeDomain);

  // Mark the peer node as inactive if it exists in our nodes table
  await db.update(nodesTable)
    .set({ status: "inactive" })
    .where(eq(nodesTable.domain, nodeDomain));

  logger.info({ nodeDomain, reason, blockedBy: req.user?.id }, "[blocklist] Node blocked");

  res.status(201).json({ block, message: `${nodeDomain} is now blocked from federating with this node.` });
}));

// ── DELETE /api/federation/blocks/:domain ────────────────────────────────────

router.delete("/federation/blocks/:domain", requireAdmin, writeLimiter, asyncHandler(async (req: Request, res: Response) => {
  const domain = (req.params.domain as string).toLowerCase();

  const [deleted] = await db.delete(federationBlocksTable)
    .where(eq(federationBlocksTable.nodeDomain, domain))
    .returning();

  if (!deleted) throw AppError.notFound(`No block found for ${domain}`);

  blockedDomains.delete(domain);

  logger.info({ domain, unblockedBy: req.user?.id }, "[blocklist] Node unblocked");

  res.json({ message: `${domain} has been unblocked and can federate with this node again.` });
}));

// ── GET /api/federation/blocks/check?domain= ─────────────────────────────────
// Public endpoint — peers can check if they're blocked before attempting handshake.
// Returns 200 with { blocked: true/false } rather than 403 so peers can handle it gracefully.

router.get("/federation/blocks/check", asyncHandler(async (req: Request, res: Response) => {
  const domain = ((req.query.domain as string) ?? "").toLowerCase();
  if (!domain) throw AppError.badRequest("domain query parameter required");

  res.json({ domain, blocked: isBlocked(domain) });
}));

export default router;
