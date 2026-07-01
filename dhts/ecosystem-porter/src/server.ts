import http from 'node:http';
import { runScan, findFreePorts } from './scan.js';
import {
  createPortReconfigurationPlan,
  findPortConflicts,
  getOutboundIntelligenceStub,
  getPortToolCapabilities,
} from './port-management.js';
import type { ScanResult } from './types.js';

const CACHE_TTL_MS = 15_000;

interface Cache {
  result: ScanResult;
  ts: number;
  probe: boolean;
}

let _cache: Cache | null = null;

async function getCached(probe: boolean, force: boolean): Promise<ScanResult> {
  const now = Date.now();
  if (!force && _cache && _cache.probe === probe && now - _cache.ts < CACHE_TTL_MS) {
    return _cache.result;
  }
  const result = await runScan({ probe, listenOnly: true });
  _cache = { result, ts: now, probe };
  return result;
}

function respond(res: http.ServerResponse, data: unknown, status = 200): void {
  const body = JSON.stringify(data, null, 2);
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    'Cache-Control': status === 200 ? `max-age=${Math.floor(CACHE_TTL_MS / 1000)}` : 'no-store',
  });
  res.end(body);
}

export function createServer(apiPort: number): http.Server {
  return http.createServer(async (req, res) => {
    // CORS preflight
    if (req.method === 'OPTIONS') {
      res.writeHead(204, { 'Access-Control-Allow-Origin': '*' });
      res.end();
      return;
    }

    if (req.method !== 'GET') {
      respond(res, { error: 'Method not allowed' }, 405);
      return;
    }

    let url: URL;
    try {
      url = new URL(req.url ?? '/', `http://localhost:${apiPort}`);
    } catch {
      respond(res, { error: 'Bad request' }, 400);
      return;
    }

    const probe = url.searchParams.get('probe') === '1' || url.searchParams.get('probe') === 'true';
    const force = url.searchParams.get('force') === '1' || url.searchParams.get('force') === 'true';
    const { pathname } = url;

    // ── /health ────────────────────────────────────────────────────────────
    if (pathname === '/health') {
      respond(res, { ok: true, ts: new Date().toISOString(), port: apiPort });
      return;
    }

    // ── / (index / summary) ────────────────────────────────────────────────
    if (pathname === '/' || pathname === '/status') {
      const result = await getCached(probe, force);
      const cacheAge = _cache ? Math.floor((Date.now() - _cache.ts) / 1000) : 0;
      respond(res, {
        service: 'nexus-porter',
        version: '1.0.0',
        hostname: result.hostname,
        scannedAt: result.scannedAt,
        cacheAgeSeconds: cacheAge,
        cacheTtlSeconds: Math.floor(CACHE_TTL_MS / 1000),
        summary: result.summary,
        endpoints: {
          'GET /':             'Service summary',
          'GET /ports':        'All listening ports (full detail)',
          'GET /ports/:n':     'Single port by number',
          'GET /scan':         'Force fresh scan (bypasses cache)',
          'GET /available':    'Free ports in range (?from=3000&to=4000)',
          'GET /capabilities': 'Implemented and planned tool capabilities',
          'GET /conflicts':    'Port conflict check (?ports=3000,5432)',
          'GET /reconfigure/plan': 'Dry-run reconfiguration plan (?from=3000&to=3100)',
          'GET /outbound':     'Outbound intelligence roadmap (stub)',
          'GET /health':       'Health check',
          'query ?probe=1':    'Enable HTTP probing (slower)',
          'query ?force=1':    'Bypass cache',
        },
      });
      return;
    }

    // ── /scan (force-refresh alias) ────────────────────────────────────────
    if (pathname === '/scan') {
      const result = await getCached(probe, true);
      respond(res, result);
      return;
    }

    // ── /ports ─────────────────────────────────────────────────────────────
    if (pathname === '/ports') {
      const result = await getCached(probe, force);
      respond(res, result);
      return;
    }

    // ── /ports/:n ─────────────────────────────────────────────────────────
    const portMatch = pathname.match(/^\/ports\/(\d+)$/);
    if (portMatch) {
      const portNum = parseInt(portMatch[1], 10);
      if (isNaN(portNum) || portNum < 1 || portNum > 65535) {
        respond(res, { error: 'Invalid port number' }, 400);
        return;
      }
      const result = await getCached(probe, force);
      const entry  = result.ports.find(p => p.port === portNum);
      if (!entry) {
        respond(res, {
          error: `Port ${portNum} is not currently listening`,
          suggestion: 'Use GET /available to find free ports',
        }, 404);
        return;
      }
      respond(res, { port: entry, scannedAt: result.scannedAt, hostname: result.hostname });
      return;
    }

    // ── /available?from=N&to=M ─────────────────────────────────────────────
    if (pathname === '/available') {
      const from = parseInt(url.searchParams.get('from') ?? '1024', 10);
      const to   = parseInt(url.searchParams.get('to')   ?? '9999', 10);
      if (isNaN(from) || isNaN(to) || from < 1 || to > 65535 || from > to) {
        respond(res, { error: 'Invalid range. Use ?from=1024&to=9999' }, 400);
        return;
      }
      if (to - from > 10000) {
        respond(res, { error: 'Range too large (max 10000 ports)' }, 400);
        return;
      }
      const free = await findFreePorts(from, to);
      respond(res, { from, to, free, count: free.length });
      return;
    }

    // ── /capabilities ───────────────────────────────────────────────────────
    if (pathname === '/capabilities') {
      respond(res, {
        capabilities: getPortToolCapabilities(),
      });
      return;
    }

    // ── /conflicts?ports=3000,5432 ─────────────────────────────────────────
    if (pathname === '/conflicts') {
      const raw = (url.searchParams.get('ports') ?? '').trim();
      const targets = raw
        .split(',')
        .map(s => parseInt(s.trim(), 10))
        .filter(n => Number.isFinite(n) && n >= 1 && n <= 65535);

      if (targets.length === 0) {
        respond(res, { error: 'Invalid or empty ports list. Use ?ports=3000,5432' }, 400);
        return;
      }

      const report = await findPortConflicts(targets);
      respond(res, report);
      return;
    }

    // ── /reconfigure/plan?from=3000&to=3100 ───────────────────────────────
    if (pathname === '/reconfigure/plan') {
      const from = parseInt(url.searchParams.get('from') ?? '', 10);
      const toRaw = parseInt(url.searchParams.get('to') ?? '', 10);
      const rangeFrom = parseInt(url.searchParams.get('rangeFrom') ?? '2000', 10);
      const rangeTo = parseInt(url.searchParams.get('rangeTo') ?? '20000', 10);

      if (!Number.isFinite(from) || from < 1 || from > 65535) {
        respond(res, { error: 'Invalid from port. Use ?from=3000' }, 400);
        return;
      }

      const to = Number.isFinite(toRaw) && toRaw >= 1 && toRaw <= 65535 ? toRaw : undefined;
      const plan = await createPortReconfigurationPlan({ fromPort: from, toPort: to, rangeFrom, rangeTo });
      respond(res, plan);
      return;
    }

    // ── /outbound (stub) ────────────────────────────────────────────────────
    if (pathname === '/outbound') {
      respond(res, getOutboundIntelligenceStub());
      return;
    }

    respond(res, {
      error: 'Not found',
      hint: 'Try GET /ports or GET /ports/:n',
    }, 404);
  });
}
