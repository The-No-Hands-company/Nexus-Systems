import { Router, type IRouter } from "express";
import { eq, count, sum, sql } from "drizzle-orm";
import { db, nodesTable, sitesTable } from "@workspace/db";
import { GetFederationStatsResponse } from "@workspace/api-zod";

const router: IRouter = Router();

router.get("/stats", async (_req, res): Promise<void> => {
  const [nodeStats] = await db
    .select({
      totalNodes: count(),
    })
    .from(nodesTable);

  const [activeNodeStats] = await db
    .select({
      activeNodes: count(),
    })
    .from(nodesTable)
    .where(eq(nodesTable.status, "active"));

  const [siteStats] = await db
    .select({
      totalSites: count(),
    })
    .from(sitesTable);

  const [activeSiteStats] = await db
    .select({
      activeSites: count(),
    })
    .from(sitesTable)
    .where(eq(sitesTable.status, "active"));

  const [capacityStats] = await db
    .select({
      totalBandwidthGb: sum(nodesTable.bandwidthCapacityGb),
      totalStorageGb: sum(nodesTable.storageCapacityGb),
    })
    .from(nodesTable);

  const [uptimeStats] = await db
    .select({
      avgUptime: sum(nodesTable.uptimePercent),
      total: count(),
    })
    .from(nodesTable)
    .where(eq(nodesTable.status, "active"));

  const totalNodes = nodeStats?.totalNodes ?? 0;
  const activeNodes = activeNodeStats?.activeNodes ?? 0;
  const totalSites = siteStats?.totalSites ?? 0;
  const activeSites = activeSiteStats?.activeSites ?? 0;
  const totalBandwidthGb = Number(capacityStats?.totalBandwidthGb ?? 0);
  const totalStorageGb = Number(capacityStats?.totalStorageGb ?? 0);
  const avgUptime = uptimeStats?.total
    ? Number(uptimeStats.avgUptime ?? 0) / Number(uptimeStats.total)
    : 0;

  res.json(
    GetFederationStatsResponse.parse({
      totalNodes,
      activeNodes,
      totalSites,
      activeSites,
      totalBandwidthGb,
      totalStorageGb,
      uptimePercent: Math.round(avgUptime * 100) / 100,
    })
  );
});

// GET /stats/hourly — returns 24 one-hour buckets of federation activity
router.get("/stats/hourly", async (_req, res): Promise<void> => {
  // Build a series of 24 hour slots ending now
  const rows = await db.execute(sql`
    SELECT
      date_trunc('hour', ts) AS hour,
      COALESCE(SUM(CASE WHEN source = 'event' THEN 1 ELSE 0 END), 0)::int  AS events,
      COALESCE(SUM(CASE WHEN source = 'deploy' THEN 1 ELSE 0 END), 0)::int AS deployments
    FROM (
      SELECT created_at AS ts, 'event'  AS source FROM federation_events
      WHERE created_at > NOW() - INTERVAL '24 hours'
      UNION ALL
      SELECT created_at AS ts, 'deploy' AS source FROM site_deployments
      WHERE created_at > NOW() - INTERVAL '24 hours'
    ) combined
    GROUP BY 1
    ORDER BY 1
  `);

  // Build complete 24-slot array (fill gaps with zeros)
  const now = new Date();
  const hourlyMap = new Map<string, { events: number; deployments: number }>();

  for (const row of rows.rows as Array<{ hour: string | Date; events: number; deployments: number }>) {
    const key = new Date(row.hour).toISOString().slice(0, 13);
    hourlyMap.set(key, { events: Number(row.events), deployments: Number(row.deployments) });
  }

  const hours = Array.from({ length: 24 }, (_, i) => {
    const d = new Date(now);
    d.setMinutes(0, 0, 0);
    d.setHours(d.getHours() - (23 - i));
    const key = d.toISOString().slice(0, 13);
    const bucket = hourlyMap.get(key) ?? { events: 0, deployments: 0 };
    return {
      hour: d.toISOString(),
      label: `${String(d.getUTCHours()).padStart(2, "0")}:00`,
      events: bucket.events,
      deployments: bucket.deployments,
      total: bucket.events + bucket.deployments,
    };
  });

  res.json({ hours });
});

export default router;
