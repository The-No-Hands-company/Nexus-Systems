/**
 * Site transfer, bulk export, and bulk import.
 *
 * Site transfer — transfer ownership to another user:
 *   POST /api/sites/:id/transfer    — initiate transfer (owner only)
 *   POST /api/sites/:id/transfer/accept — accept transfer (new owner, authenticated)
 *
 * Bulk export — download all site data as a zip-compatible JSON manifest:
 *   GET  /api/sites/:id/export      — export site metadata + file manifest
 *
 * Bulk import — recreate a site from an export manifest:
 *   POST /api/sites/import          — import from export manifest
 */

import { Router, type IRouter, type Request, type Response } from "express";
import { z } from "zod/v4";
import crypto from "crypto";
import { db, sitesTable, siteFilesTable, siteDeploymentsTable, siteRedirectRulesTable, siteCustomHeadersTable, usersTable } from "@workspace/db";
import { eq, and, desc } from "drizzle-orm";
import { asyncHandler, AppError } from "../lib/errors";
import { writeLimiter } from "../middleware/rateLimiter";
import { storage } from "../lib/storageProvider";
import { invalidateSiteCache } from "../lib/domainCache";
import logger from "../lib/logger";

const router: IRouter = Router();

import { getRedisClient } from "../lib/redis";

const TRANSFER_TTL = 24 * 60 * 60; // 24 hours in seconds
const TRANSFER_PREFIX = "site_transfer:";

async function storeTransfer(token: string, data: object): Promise<void> {
  const redis = getRedisClient();
  if (redis) {
    await redis.set(`${TRANSFER_PREFIX}${token}`, JSON.stringify(data), "EX", TRANSFER_TTL);
  }
  // Always also store in memory as fallback when Redis is not available
  _fallbackTransfers.set(token, { ...(data as any), createdAt: Date.now() });
}

async function getTransfer(token: string): Promise<{ siteId: number; fromUserId: string; toEmail: string; createdAt: number } | null> {
  const redis = getRedisClient();
  if (redis) {
    const raw = await redis.get(`${TRANSFER_PREFIX}${token}`).catch(() => null);
    if (raw) return JSON.parse(raw);
  }
  // Fallback to memory
  const mem = _fallbackTransfers.get(token);
  if (!mem) return null;
  if (Date.now() - mem.createdAt > TRANSFER_TTL * 1000) {
    _fallbackTransfers.delete(token);
    return null;
  }
  return mem;
}

async function deleteTransfer(token: string): Promise<void> {
  const redis = getRedisClient();
  if (redis) await redis.del(`${TRANSFER_PREFIX}${token}`).catch(() => {});
  _fallbackTransfers.delete(token);
}

// In-memory fallback when Redis is not available
const _fallbackTransfers = new Map<string, { siteId: number; fromUserId: string; toEmail: string; createdAt: number }>();

// ── Transfer ownership ─────────────────────────────────────────────────────────

router.post("/sites/:id/transfer", writeLimiter, asyncHandler(async (req: Request, res: Response) => {
  if (!req.isAuthenticated()) throw AppError.unauthorized();
  const siteId = parseInt(req.params.id as string, 10);
  if (isNaN(siteId)) throw AppError.badRequest("Invalid site ID");

  const { toEmail } = z.object({ toEmail: z.string().email() }).parse(req.body);

  const [site] = await db.select({ ownerId: sitesTable.ownerId, name: sitesTable.name, domain: sitesTable.domain })
    .from(sitesTable).where(eq(sitesTable.id, siteId));
  if (!site) throw AppError.notFound("Site not found");
  if (site.ownerId !== req.user.id) throw AppError.forbidden("Only the site owner can initiate a transfer");

  if (toEmail === req.user.email) throw AppError.badRequest("You cannot transfer a site to yourself");

  // Check recipient exists
  const [recipient] = await db.select({ id: usersTable.id, email: usersTable.email })
    .from(usersTable).where(eq(usersTable.email, toEmail));
  if (!recipient) throw AppError.notFound(`No user found with email ${toEmail}. They must have an account first.`);

  const token = crypto.randomBytes(32).toString("base64url");
  await storeTransfer(token, { siteId, fromUserId: req.user.id, toEmail, createdAt: Date.now() });

  logger.info({ siteId, from: req.user.id, to: toEmail }, "[transfer] Initiated");

  res.json({
    token,
    toEmail,
    siteName: site.name,
    domain: site.domain,
    expiresIn: "24 hours",
    message: `Send the recipient this token: ${token}. They must call POST /api/sites/${siteId}/transfer/accept with it.`,
    ...(process.env.NODE_ENV !== "production" ? { acceptUrl: `/api/sites/${siteId}/transfer/accept` } : {}),
  });
}));

router.post("/sites/:id/transfer/accept", writeLimiter, asyncHandler(async (req: Request, res: Response) => {
  if (!req.isAuthenticated()) throw AppError.unauthorized();
  const siteId = parseInt(req.params.id as string, 10);
  if (isNaN(siteId)) throw AppError.badRequest("Invalid site ID");

  const { token } = z.object({ token: z.string() }).parse(req.body);

  const transfer = await getTransfer(token);
  if (!transfer) throw AppError.notFound("Transfer token not found or expired");
  if (transfer.siteId !== siteId) throw AppError.badRequest("Token is for a different site");

  if (req.user.email !== transfer.toEmail) {
    throw AppError.forbidden("This transfer was sent to a different email address");
  }

  await db.update(sitesTable)
    .set({ ownerId: req.user.id, ownerEmail: req.user.email ?? "", ownerName: req.user.firstName ?? req.user.email ?? "" })
    .where(eq(sitesTable.id, siteId));

  await deleteTransfer(token);
  invalidateSiteCache(siteId);

  logger.info({ siteId, newOwner: req.user.id }, "[transfer] Completed");
  res.json({ transferred: true, newOwnerId: req.user.id });
}));

// ── Export ─────────────────────────────────────────────────────────────────────

router.get("/sites/:id/export", asyncHandler(async (req: Request, res: Response) => {
  if (!req.isAuthenticated()) throw AppError.unauthorized();
  const siteId = parseInt(req.params.id as string, 10);
  if (isNaN(siteId)) throw AppError.badRequest("Invalid site ID");

  const [site] = await db.select().from(sitesTable).where(eq(sitesTable.id, siteId));
  if (!site) throw AppError.notFound("Site not found");
  if (site.ownerId !== req.user.id) throw AppError.forbidden();

  const [activeDep] = await db.select().from(siteDeploymentsTable)
    .where(and(eq(siteDeploymentsTable.siteId, siteId), eq(siteDeploymentsTable.status, "active")));

  const files = activeDep ? await db.select().from(siteFilesTable)
    .where(and(eq(siteFilesTable.siteId, siteId), eq(siteFilesTable.deploymentId, activeDep.id))) : [];

  const redirectRules = await db.select().from(siteRedirectRulesTable)
    .where(eq(siteRedirectRulesTable.siteId, siteId));

  const customHeaders = await db.select().from(siteCustomHeadersTable)
    .where(eq(siteCustomHeadersTable.siteId, siteId));

  // Generate presigned download URLs (valid 2 hours)
  const filesWithUrls = await Promise.allSettled(files.map(async (f) => ({
    filePath:    f.filePath,
    contentType: f.contentType,
    sizeBytes:   f.sizeBytes,
    contentHash: f.contentHash,
    downloadUrl: await storage.getDownloadUrl(f.objectPath, 7200),
  })));

  const manifest = {
    exportVersion: "1.0",
    exportedAt: new Date().toISOString(),
    site: {
      name: site.name, domain: site.domain, description: site.description,
      siteType: site.siteType, visibility: site.visibility,
    },
    deployment: activeDep ? {
      version: activeDep.version, fileCount: activeDep.fileCount, totalSizeMb: activeDep.totalSizeMb,
    } : null,
    files: filesWithUrls.filter(r => r.status === "fulfilled").map(r => (r as any).value),
    redirectRules: redirectRules.map(({ id: _, siteId: __, createdAt: ___, ...r }) => r),
    customHeaders: customHeaders.map(({ id: _, siteId: __, createdAt: ___, ...h }) => h),
    downloadUrlExpiry: new Date(Date.now() + 7200_000).toISOString(),
  };

  res.setHeader("Content-Disposition", `attachment; filename="${site.domain}-export.json"`);
  res.json(manifest);
}));

// ── Import ─────────────────────────────────────────────────────────────────────

router.post("/sites/import", writeLimiter, asyncHandler(async (req: Request, res: Response) => {
  if (!req.isAuthenticated()) throw AppError.unauthorized();

  const ManifestSchema = z.object({
    exportVersion: z.string(),
    site: z.object({
      name: z.string(), domain: z.string(), description: z.string().optional(),
      siteType: z.enum(["static", "dynamic", "blog", "portfolio", "other"]).default("static"),
    }),
    files: z.array(z.object({
      filePath: z.string(), contentType: z.string(),
      sizeBytes: z.number(), downloadUrl: z.string().url(),
      contentHash: z.string().optional(),
    })).max(10_000),
    redirectRules: z.array(z.object({
      src: z.string(), dest: z.string(), status: z.number(), force: z.number(), position: z.number(),
    })).optional().default([]),
    customHeaders: z.array(z.object({
      path: z.string(), name: z.string(), value: z.string(),
    })).optional().default([]),
  });

  const manifest = ManifestSchema.safeParse(req.body);
  if (!manifest.success) throw AppError.badRequest(manifest.error.message);

  const { site: siteData, files, redirectRules, customHeaders } = manifest.data;

  // Check domain not already taken
  const [existing] = await db.select({ id: sitesTable.id }).from(sitesTable).where(eq(sitesTable.domain, siteData.domain));
  if (existing) throw AppError.conflict(`Domain ${siteData.domain} is already registered`);

  const [newSite] = await db.insert(sitesTable).values({
    name: siteData.name, domain: siteData.domain, description: siteData.description ?? "",
    siteType: siteData.siteType, ownerId: req.user.id,
    ownerName: req.user.firstName ?? req.user.email ?? "", ownerEmail: req.user.email ?? "",
  }).returning();

  logger.info({ siteId: newSite.id, domain: siteData.domain, fileCount: files.length }, "[import] Starting");

  // Create deployment record
  const [dep] = await db.insert(siteDeploymentsTable).values({
    siteId: newSite.id, version: 1, deployedBy: `import:${req.user.id}`,
    environment: "production", status: "pending", fileCount: files.length,
  }).returning();

  // Download + re-upload each file to this node's storage
  let imported = 0, skipped = 0;
  for (const file of files) {
    try {
      const { uploadUrl, objectPath } = await storage.getUploadUrl({ contentType: file.contentType, ttlSec: 900 });
      const fileData = await fetch(file.downloadUrl, { signal: AbortSignal.timeout(30_000) });
      if (!fileData.ok) { skipped++; continue; }
      await fetch(uploadUrl, { method: "PUT", headers: { "Content-Type": file.contentType }, body: fileData.body, duplex: "half" } as any);
      await db.insert(siteFilesTable).values({
        siteId: newSite.id, filePath: file.filePath, objectPath,
        contentType: file.contentType, sizeBytes: file.sizeBytes,
        contentHash: file.contentHash ?? null, deploymentId: dep.id,
      });
      imported++;
    } catch { skipped++; }
  }

  // Activate deployment
  await db.update(siteDeploymentsTable).set({ status: "active" }).where(eq(siteDeploymentsTable.id, dep.id));
  await db.update(sitesTable).set({ storageUsedMb: files.reduce((a, f) => a + f.sizeBytes, 0) / (1024 * 1024) }).where(eq(sitesTable.id, newSite.id));

  // Import redirect rules and headers
  if (redirectRules.length > 0) {
    await db.insert(siteRedirectRulesTable).values(redirectRules.map(r => ({ ...r, siteId: newSite.id })));
  }
  if (customHeaders.length > 0) {
    await db.insert(siteCustomHeadersTable).values(customHeaders.map(h => ({ ...h, siteId: newSite.id })));
  }

  logger.info({ siteId: newSite.id, imported, skipped }, "[import] Complete");
  res.status(201).json({ siteId: newSite.id, domain: siteData.domain, filesImported: imported, filesSkipped: skipped });
}));

export default router;
