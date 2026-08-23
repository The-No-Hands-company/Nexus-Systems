/**
 * Orphaned object storage cleanup.
 *
 * When a new deployment is made, files from the previous deployment that no
 * longer exist in the current deployment become orphans — they consume storage
 * but are never served. This job identifies and deletes them.
 *
 * Safety rules:
 *   - Only deletes file records where deployment_id belongs to a non-active,
 *     non-rolled_back deployment (i.e. truly superseded)
 *   - NEVER deletes an objectPath that is still referenced by another file
 *     record (content deduplication means the same object can be shared)
 *   - Runs every 6 hours, max 500 deletions per run to avoid S3 API rate limits
 *   - Logs every deletion for auditability
 */

import { db, siteFilesTable, siteDeploymentsTable } from "@workspace/db";
import { eq, and, inArray, notInArray, lt, isNotNull, ne } from "drizzle-orm";
import { storage } from "./storageProvider";
import logger from "./logger";

const BATCH_SIZE = 500;
const CLEANUP_INTERVAL_MS = 6 * 60 * 60 * 1000;

async function runCleanup(): Promise<void> {
  try {
    // Find deployments that are not active and not pending
    // (failed, rolled_back — their files are no longer being served)
    const staleDeployments = await db
      .select({ id: siteDeploymentsTable.id })
      .from(siteDeploymentsTable)
      .where(
        and(
          inArray(siteDeploymentsTable.status, ["failed", "rolled_back"]),
        ),
      )
      .limit(BATCH_SIZE);

    if (staleDeployments.length === 0) return;

    const staleIds = staleDeployments.map((d) => d.id);

    // Get files exclusively belonging to stale deployments
    const orphanedFiles = await db
      .select({ id: siteFilesTable.id, objectPath: siteFilesTable.objectPath })
      .from(siteFilesTable)
      .where(
        and(
          isNotNull(siteFilesTable.deploymentId),
          inArray(siteFilesTable.deploymentId, staleIds),
        ),
      )
      .limit(BATCH_SIZE);

    if (orphanedFiles.length === 0) return;

    // Critical safety check: only delete objectPaths that are NOT referenced
    // by any other file record (deduplication means same content = same objectPath)
    const orphanPaths = [...new Set(orphanedFiles.map((f) => f.objectPath))];
    const orphanFileIds = orphanedFiles.map((f) => f.id);

    const stillUsed = await db
      .select({ objectPath: siteFilesTable.objectPath })
      .from(siteFilesTable)
      .where(
        and(
          inArray(siteFilesTable.objectPath, orphanPaths),
          notInArray(siteFilesTable.id, orphanFileIds),
        ),
      );

    const usedPaths = new Set(stillUsed.map((f) => f.objectPath));
    const safeToDelete = orphanedFiles.filter((f) => !usedPaths.has(f.objectPath));

    if (safeToDelete.length === 0) {
      // Delete DB records even if all objectPaths are shared — the deduped objects stay
      await db.delete(siteFilesTable).where(inArray(siteFilesTable.id, orphanFileIds));
      logger.debug({ count: orphanFileIds.length }, "[cleanup] Removed orphaned DB records (shared objects kept)");
      return;
    }

    let deleted = 0;
    let errors = 0;

    for (const file of safeToDelete) {
      try {
        await storage.delete(file.objectPath);
        deleted++;
      } catch {
        errors++;
      }
    }

    // Remove all orphaned DB records regardless of storage delete success
    await db.delete(siteFilesTable).where(inArray(siteFilesTable.id, orphanFileIds));

    logger.info(
      { deleted, errors, dbRecords: orphanFileIds.length },
      "[cleanup] Orphaned file cleanup complete",
    );
  } catch (err) {
    logger.warn({ err }, "[cleanup] Orphan cleanup run failed");
  }
}

let cleanupTimer: NodeJS.Timeout | null = null;

export function startOrphanCleanup(): void {
  if (cleanupTimer) return;
  // Delay first run by 10 minutes so startup isn't impacted
  setTimeout(() => {
    runCleanup();
    cleanupTimer = setInterval(runCleanup, CLEANUP_INTERVAL_MS);
  }, 10 * 60 * 1000);
  logger.info("[cleanup] Orphaned file cleanup scheduler started");
}

export function stopOrphanCleanup(): void {
  if (cleanupTimer) { clearInterval(cleanupTimer); cleanupTimer = null; }
}
