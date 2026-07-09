import { IncomingMessage, ServerResponse } from "http";
import { Config } from "../lib/config";
import { logger } from "../lib/logger";

interface RequestContext {
  requestId: string;
  clientIp: string;
  region?: string;
  metadata: Record<string, unknown>;
}

/**
 * IP Router: Determine geographic region and route to appropriate cloud instance
 */
export async function ipRouter(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config
): Promise<void> {
  try {
    const { clientIp } = context;
    const geoConfig = config.geography;

    if (!geoConfig) {
      // No geography config, continue
      return;
    }

    // Determine region from client IP
    const region = determineRegion(clientIp, geoConfig);
    context.region = region;
    context.metadata.region = region;

    logger.debug(
      { request_id: context.requestId, client_ip: clientIp, region },
      "IP router: determined region"
    );

    // If routing to a different cloud, add header for upstream
    const cloudUrl = geoConfig.regions?.[region]?.cloud_url;
    if (cloudUrl && cloudUrl !== config.cloud?.url) {
      context.metadata.upstream_cloud_url = cloudUrl;
      req.headers["x-nexus-cloud-url"] = cloudUrl;
    }
  } catch (err) {
    logger.warn(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "IP router error (continuing)"
    );
  }
}

function determineRegion(
  clientIp: string,
  geoConfig: any
): string {
  if (!geoConfig?.regions) {
    return geoConfig?.default_region || "us-east-1";
  }

  // Simple CIDR matching
  for (const [region, regionConfig] of Object.entries(geoConfig.regions)) {
    const cidrs = (regionConfig as any)?.cidrs || [];
    for (const cidr of cidrs) {
      if (ipInCidr(clientIp, cidr as string)) {
        return region;
      }
    }
  }

  return geoConfig.default_region || "us-east-1";
}

function ipInCidr(ip: string, cidr: string): boolean {
  // Simplified: just check prefix for demo
  // In production, use proper CIDR library
  const [network, bits] = cidr.split("/");
  const prefixLen = parseInt(bits, 10);
  const ipParts = ip.split(".").map(Number);
  const netParts = network.split(".").map(Number);

  const bytesToCheck = Math.ceil(prefixLen / 8);
  for (let i = 0; i < bytesToCheck; i++) {
    if (ipParts[i] !== netParts[i]) {
      return false;
    }
  }

  return true;
}
