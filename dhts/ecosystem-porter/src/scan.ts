import os from 'node:os';
import { scanSockets, buildInodeMap, getProcessInfo, ssLookup } from './scanner.js';
import { probePort } from './prober.js';
import { getContainerPorts } from './containers.js';
import { lookupPort } from './port-db.js';
import type { PortEntry, ScanResult, Proto } from './types.js';

export interface ScanOptions {
  /** Run HTTP probes for each TCP port (slower, default: false) */
  probe?: boolean;
  /** Include only LISTEN/active UDP sockets (default: true) */
  listenOnly?: boolean;
  /** Exclude this port from results (e.g. porter's own API port) */
  selfPort?: number;
}

export async function runScan(options: ScanOptions = {}): Promise<ScanResult> {
  const { probe = false, listenOnly = true, selfPort } = options;

  // ── 1. Raw sockets ────────────────────────────────────────────────────────
  const sockets = scanSockets(listenOnly);

  // ── 2. Inode → PID map ───────────────────────────────────────────────────
  const inodeMap = buildInodeMap();

  // Fallback: if inode map is empty (non-root, restricted proc access), use `ss`
  const ssFallback = inodeMap.size === 0 ? ssLookup() : new Map();

  // ── 3. Container ports ────────────────────────────────────────────────────
  const containerPorts = getContainerPorts();

  // ── 4. Aggregate by port (deduplicate tcp + tcp6 for the same port) ───────
  const portMap = new Map<number, PortEntry>();

  for (const sock of sockets) {
    const { localPort: port, localAddress: bind, state, uid, inode, proto } = sock;

    if (selfPort !== undefined && port === selfPort) continue;
    if (port === 0) continue;

    if (portMap.has(port)) {
      // Merge additional protocol variant
      const existing = portMap.get(port)!;
      if (!existing.protocols.includes(proto)) existing.protocols.push(proto);
      continue;
    }

    // Resolve process
    let process = null;
    const pid = inodeMap.get(inode) ?? null;
    if (pid !== null) {
      process = getProcessInfo(pid, uid);
    } else if (ssFallback.has(port)) {
      const ss = ssFallback.get(port)!;
      process = getProcessInfo(ss.pid, 0) ?? {
        pid: ss.pid,
        name: ss.name,
        cmdline: ss.name,
        user: null,
        cgroup: null,
      };
    }

    portMap.set(port, {
      port,
      proto,
      state,
      bind: normalizeAddress(bind),
      process,
      probe: null,
      known: lookupPort(port),
      container: containerPorts.get(port) ?? null,
      protocols: [proto],
    });
  }

  // Ports that appear only in podman/docker but not via /proc (pure container ports)
  for (const [hostPort, cp] of containerPorts) {
    if (portMap.has(hostPort)) continue;
    if (selfPort !== undefined && hostPort === selfPort) continue;
    portMap.set(hostPort, {
      port: hostPort,
      proto: (cp.protocol === 'udp' ? 'udp' : 'tcp') as Proto,
      state: 'LISTEN',
      bind: '0.0.0.0',
      process: null,
      probe: null,
      known: lookupPort(hostPort),
      container: cp,
      protocols: [(cp.protocol === 'udp' ? 'udp' : 'tcp') as Proto],
    });
  }

  // ── 5. Sort by port number ────────────────────────────────────────────────
  const entries = Array.from(portMap.values()).sort((a, b) => a.port - b.port);

  // ── 6. Optional HTTP probing ──────────────────────────────────────────────
  if (probe) {
    await Promise.all(
      entries
        .filter(e => e.protocols.some(p => p === 'tcp' || p === 'tcp6'))
        .map(async (entry) => {
          entry.probe = await probePort(entry.port);
        }),
    );
  }

  // ── 7. Summary ────────────────────────────────────────────────────────────
  const summary = {
    totalListening: entries.length,
    httpReachable:  entries.filter(e => e.probe?.reachable === true).length,
    notEmbeddable:  entries.filter(e => e.probe?.reachable === true && e.probe.embeddable === false).length,
    withProcess:    entries.filter(e => e.process !== null).length,
    inContainers:   entries.filter(e => e.container !== null).length,
  };

  return {
    scannedAt: new Date().toISOString(),
    hostname: os.hostname(),
    probeEnabled: probe,
    ports: entries,
    summary,
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

function normalizeAddress(addr: string): string {
  if (addr === '::' || addr === '0.0.0.0' || addr === '0000:0000:0000:0000:0000:0000:0000:0000')
    return '0.0.0.0';
  if (addr === '::1') return '::1';
  // IPv4-mapped IPv6 ::ffff:127.0.0.1 → 127.0.0.1
  const ipv4Mapped = addr.match(/^::ffff:(\d+\.\d+\.\d+\.\d+)$/i);
  if (ipv4Mapped) return ipv4Mapped[1];
  return addr;
}

/**
 * Return the list of port numbers within [from, to] that are NOT currently listening.
 */
export async function findFreePorts(from: number, to: number): Promise<number[]> {
  const result = await runScan({ listenOnly: true });
  const used = new Set(result.ports.map(p => p.port));
  const free: number[] = [];
  for (let p = from; p <= to; p++) {
    if (!used.has(p)) free.push(p);
  }
  return free;
}
