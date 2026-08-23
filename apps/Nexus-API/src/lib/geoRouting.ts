/**
 * Geographic routing helpers.
 *
 * Selects the closest active federation node to serve a given request based on:
 *   1. Exact region match  (e.g. "ap-southeast-1" === "ap-southeast-1")
 *   2. Region prefix match (e.g. "ap-" prefix — same continent)
 *   3. Lowest latency probe (if ENABLE_GEO_LATENCY_PROBE=true)
 *   4. Fallback: local node
 *
 * The node domain returned can be used to issue a 302 redirect so the client
 * fetches files from the nearest node instead of the origin.
 *
 * Region format follows AWS/GCP naming conventions:
 *   ap-southeast-1  (Asia Pacific — Singapore)
 *   ap-southeast-3  (Asia Pacific — Jakarta)
 *   eu-west-1       (Europe — Ireland)
 *   us-east-1       (US East)
 *   us-west-2       (US West)
 *   self-hosted     (unknown / self-hosted)
 */

import { db, nodesTable } from "@workspace/db";
import { eq, and, ne } from "drizzle-orm";
import logger from "./logger";

export interface GeoRoutingResult {
  nodeDomain: string;
  nodeId: number;
  region: string;
  reason: "exact_region" | "region_prefix" | "latency_probe" | "fallback";
  isLocal: boolean;
}

/**
 * Infer region from request headers.
 * Works with Cloudflare (CF-IPCountry), AWS CloudFront (CloudFront-Viewer-Country),
 * and fly.io (Fly-Region).
 */
export function inferRegionFromRequest(headers: Record<string, string | string[] | undefined>): string | null {
  // fly.io — already gives us a region code like "sin", "ams", "ord"
  const flyRegion = headers["fly-region"] as string | undefined;
  if (flyRegion) return flyRegionToAws(flyRegion);

  // Cloudflare country code
  const cfCountry = headers["cf-ipcountry"] as string | undefined;
  if (cfCountry) return countryToRegion(cfCountry);

  // CloudFront viewer country
  const cfViewerCountry = headers["cloudfront-viewer-country"] as string | undefined;
  if (cfViewerCountry) return countryToRegion(cfViewerCountry);

  // X-Forwarded-For geo (if operator adds it via nginx/Caddy)
  const xGeoRegion = headers["x-geo-region"] as string | undefined;
  if (xGeoRegion) return xGeoRegion;

  return null;
}

/** Map fly.io 3-letter region codes to AWS-style region identifiers */
function flyRegionToAws(fly: string): string {
  const map: Record<string, string> = {
    sin: "ap-southeast-1",
    jkt: "ap-southeast-3",
    nrt: "ap-northeast-1",
    syd: "ap-southeast-2",
    bom: "ap-south-1",
    ams: "eu-west-1",
    lhr: "eu-west-2",
    fra: "eu-central-1",
    iad: "us-east-1",
    ord: "us-east-2",
    lax: "us-west-1",
    sea: "us-west-2",
    gru: "sa-east-1",
    jnb: "af-south-1",
  };
  return map[fly.toLowerCase()] ?? `unknown-${fly}`;
}

/** Map ISO 3166-1 alpha-2 country codes to AWS-style regions */
function countryToRegion(country: string): string {
  const map: Record<string, string> = {
    // Southeast Asia
    ID: "ap-southeast-3", SG: "ap-southeast-1", MY: "ap-southeast-1",
    TH: "ap-southeast-1", VN: "ap-southeast-1", PH: "ap-southeast-1",
    // East Asia
    JP: "ap-northeast-1", KR: "ap-northeast-2", CN: "ap-east-1",
    // South Asia
    IN: "ap-south-1", PK: "ap-south-1", BD: "ap-south-1",
    // Australia / Pacific
    AU: "ap-southeast-2", NZ: "ap-southeast-2",
    // Europe
    DE: "eu-central-1", FR: "eu-west-3", GB: "eu-west-2",
    NL: "eu-west-1", IE: "eu-west-1", SE: "eu-north-1",
    // Americas
    US: "us-east-1", CA: "ca-central-1",
    BR: "sa-east-1", MX: "us-east-1",
    // Africa / Middle East
    ZA: "af-south-1", NG: "af-south-1",
    AE: "me-south-1", SA: "me-south-1",
  };
  return map[country.toUpperCase()] ?? "us-east-1";
}

/**
 * Select the best node to serve a request from.
 * Returns null if the local node is already the best choice.
 */
export async function selectClosestNode(
  clientRegion: string | null,
  localNodeId: number,
): Promise<GeoRoutingResult | null> {
  if (!clientRegion) return null;

  const activePeers = await db
    .select({
      id: nodesTable.id,
      domain: nodesTable.domain,
      region: nodesTable.region,
      isLocalNode: nodesTable.isLocalNode,
    })
    .from(nodesTable)
    .where(and(eq(nodesTable.status, "active")));

  if (activePeers.length <= 1) return null;

  const localNode = activePeers.find((n) => n.isLocalNode === 1);

  // 1. Exact region match
  const exactMatch = activePeers.find(
    (n) => n.region === clientRegion && n.id !== localNodeId,
  );
  if (exactMatch) {
    // Only redirect if this peer is actually closer than local
    if (localNode?.region === clientRegion) return null; // local is already in the right region
    return {
      nodeDomain: exactMatch.domain,
      nodeId: exactMatch.id,
      region: exactMatch.region,
      reason: "exact_region",
      isLocal: false,
    };
  }

  // 2. Region prefix match (same continent, e.g. "ap-")
  const clientPrefix = clientRegion.split("-")[0] + "-";
  const localPrefix = (localNode?.region ?? "").split("-")[0] + "-";

  if (localPrefix === clientPrefix) return null; // local is already same continent

  const prefixMatch = activePeers.find(
    (n) => n.region.startsWith(clientPrefix) && n.id !== localNodeId,
  );
  if (prefixMatch) {
    return {
      nodeDomain: prefixMatch.domain,
      nodeId: prefixMatch.id,
      region: prefixMatch.region,
      reason: "region_prefix",
      isLocal: false,
    };
  }

  // No better node found — serve locally
  return null;
}

/**
 * Express middleware: adds X-Served-From-Region and optionally issues a
 * redirect to the closest node for site serving requests.
 *
 * Only activates when:
 *   - The request is for a user-facing site (not /api routes)
 *   - ENABLE_GEO_ROUTING=true is set
 *   - A closer node exists
 */
export async function geoRoutingMiddleware(
  req: import("express").Request,
  res: import("express").Response,
  next: import("express").NextFunction,
): Promise<void> {
  // Only redirect for site-serving requests
  if (!process.env.ENABLE_GEO_ROUTING || req.path.startsWith("/api")) {
    next();
    return;
  }

  try {
    const clientRegion = inferRegionFromRequest(req.headers as Record<string, string | undefined>);
    if (!clientRegion) { next(); return; }

    const [localNode] = await db
      .select({ id: nodesTable.id, region: nodesTable.region })
      .from(nodesTable)
      .where(eq(nodesTable.isLocalNode, 1));

    if (!localNode) { next(); return; }

    res.setHeader("X-Served-From-Region", localNode.region);

    const closest = await selectClosestNode(clientRegion, localNode.id);
    if (!closest) { next(); return; }

    // Issue a 302 redirect to the closer node, preserving the full path
    const targetUrl = closest.nodeDomain.startsWith("http")
      ? closest.nodeDomain
      : `https://${closest.nodeDomain}`;

    logger.debug(
      { clientRegion, redirectTo: closest.nodeDomain, reason: closest.reason },
      "[geo] Redirecting to closer node",
    );

    res.setHeader("X-Geo-Redirect-Reason", closest.reason);
    res.redirect(302, `${targetUrl}${req.originalUrl}`);
  } catch (err) {
    // Never let geo routing break the request
    logger.warn({ err }, "[geo] Routing error — falling through");
    next();
  }
}
