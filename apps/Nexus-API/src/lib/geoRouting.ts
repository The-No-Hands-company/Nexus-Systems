/**
 * Geographic routing — deliberately a no-op beyond labelling.
 *
 * Choosing the nearest node means knowing roughly where the client is, which
 * means reading something derived from its network address (a GeoIP country
 * header, a CDN region, a latency probe from its IP). This service no longer
 * sees, derives or stores anything of the kind: the ecosystem proxy strips
 * address headers and hands us only an opaque client tag. So every request is
 * served by the default (local) node, and any cross-region placement is the
 * edge's job, not ours.
 */

import { db, nodesTable } from "@workspace/db";
import { eq } from "drizzle-orm";
import logger from "./logger";

/**
 * Express middleware: advertises which region served the response. Never
 * redirects, and never reads the request's headers.
 */
export async function geoRoutingMiddleware(
  req: import("express").Request,
  res: import("express").Response,
  next: import("express").NextFunction,
): Promise<void> {
  if (!process.env.ENABLE_GEO_ROUTING || req.path.startsWith("/api")) {
    next();
    return;
  }
  try {
    const [localNode] = await db
      .select({ region: nodesTable.region })
      .from(nodesTable)
      .where(eq(nodesTable.isLocalNode, 1));
    if (localNode) res.setHeader("X-Served-From-Region", localNode.region);
  } catch (err) {
    logger.warn({ err }, "[geo] Region lookup failed — continuing");
  }
  next();
}
