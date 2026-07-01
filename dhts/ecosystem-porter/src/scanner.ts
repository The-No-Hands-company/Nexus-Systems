import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import type { Proto, RawSocket, ProcessInfo } from './types.js';

// ─────────────────────────────────────────────────────────────────────────────
// Platform detection
// ─────────────────────────────────────────────────────────────────────────────

const IS_LINUX = os.platform() === 'linux';
const IS_MACOS = os.platform() === 'darwin';

// ─────────────────────────────────────────────────────────────────────────────
// /proc/net/* parsing (Linux only)
// ─────────────────────────────────────────────────────────────────────────────

const TCP_STATES: Record<string, string> = {
  '01': 'ESTABLISHED',
  '02': 'SYN_SENT',
  '03': 'SYN_RECV',
  '04': 'FIN_WAIT1',
  '05': 'FIN_WAIT2',
  '06': 'TIME_WAIT',
  '07': 'CLOSE',
  '08': 'CLOSE_WAIT',
  '09': 'LAST_ACK',
  '0A': 'LISTEN',
  '0B': 'CLOSING',
  '0C': 'NEW_SYN_RECV',
};

/**
 * Parse an IPv4 address from the /proc/net/tcp hex format.
 * The 8-char hex string represents a 32-bit LE integer.
 * e.g. "0100007F" → 127.0.0.1
 */
function parseIPv4Hex(hex: string): string {
  if (hex === '00000000') return '0.0.0.0';
  return [
    parseInt(hex.slice(6, 8), 16),
    parseInt(hex.slice(4, 6), 16),
    parseInt(hex.slice(2, 4), 16),
    parseInt(hex.slice(0, 2), 16),
  ].join('.');
}

/**
 * Parse an IPv6 address from the /proc/net/tcp6 hex format.
 * The 32-char hex string is 4 × 32-bit LE words in network-byte-order after reversal.
 */
function parseIPv6Hex(hex: string): string {
  if (hex === '00000000000000000000000000000000') return '::';
  if (hex === '00000000000000000000000001000000') return '::1';

  const groups: number[] = [];
  for (let i = 0; i < 32; i += 8) {
    const g = hex.slice(i, i + 8);
    // Each 8-char group is a 32-bit LE integer; reverse bytes to get network order
    const b = [
      parseInt(g.slice(6, 8), 16),
      parseInt(g.slice(4, 6), 16),
      parseInt(g.slice(2, 4), 16),
      parseInt(g.slice(0, 2), 16),
    ];
    groups.push((b[0] << 8) | b[1]);
    groups.push((b[2] << 8) | b[3]);
  }

  const hexGroups = groups.map(g => (g >>> 0).toString(16));

  // Find longest run of consecutive zero groups for :: compression
  let bestStart = -1, bestLen = 0, curStart = -1, curLen = 0;
  for (let i = 0; i < 8; i++) {
    if (hexGroups[i] === '0') {
      if (curStart === -1) { curStart = i; curLen = 1; }
      else curLen++;
      if (curLen > bestLen) { bestLen = curLen; bestStart = curStart; }
    } else {
      curStart = -1; curLen = 0;
    }
  }

  if (bestLen >= 2) {
    const left  = hexGroups.slice(0, bestStart).join(':');
    const right = hexGroups.slice(bestStart + bestLen).join(':');
    if (!left && !right) return '::';
    if (!left)  return '::' + right;
    if (!right) return left + '::';
    return left + '::' + right;
  }

  return hexGroups.join(':');
}

function parseNetFile(filePath: string, proto: Proto): RawSocket[] {
  let content: string;
  try {
    content = fs.readFileSync(filePath, 'utf8');
  } catch {
    return [];
  }

  const result: RawSocket[] = [];
  const lines = content.trim().split('\n');
  // Skip header line (index 0)
  for (let i = 1; i < lines.length; i++) {
    const fields = lines[i].trim().split(/\s+/);
    if (fields.length < 10) continue;

    const [localAddrHex, localPortHex] = fields[1].split(':');
    const [remAddrHex,   remPortHex]   = fields[2].split(':');
    const stateHex = fields[3].toUpperCase();
    const uid   = parseInt(fields[7], 10);
    const inode = parseInt(fields[9], 10);

    if (!localAddrHex || !localPortHex) continue;

    const localPort  = parseInt(localPortHex, 16);
    const remotePort = parseInt(remPortHex ?? '0', 16);

    const isV6 = proto === 'tcp6' || proto === 'udp6';
    const localAddress  = isV6 ? parseIPv6Hex(localAddrHex)  : parseIPv4Hex(localAddrHex);
    const remoteAddress = isV6 ? parseIPv6Hex(remAddrHex ?? '00000000000000000000000000000000')
                                : parseIPv4Hex(remAddrHex ?? '00000000');

    const state = TCP_STATES[stateHex] ?? stateHex;

    result.push({ localAddress, localPort, remoteAddress, remotePort, state, uid, inode, proto });
  }
  return result;
}

export function scanSockets(listenOnly = true): RawSocket[] {
  // macOS / BSD fallback
  if (!IS_LINUX) {
    return macosScanSockets(listenOnly);
  }

  const all: RawSocket[] = [
    ...parseNetFile('/proc/net/tcp',  'tcp'),
    ...parseNetFile('/proc/net/tcp6', 'tcp6'),
    ...parseNetFile('/proc/net/udp',  'udp'),
    ...parseNetFile('/proc/net/udp6', 'udp6'),
  ];

  if (!listenOnly) return all;

  return all.filter(s =>
    s.state === 'LISTEN' ||
    (s.proto === 'udp'  && s.inode > 0) ||
    (s.proto === 'udp6' && s.inode > 0)
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// Process resolution: inode → PID → ProcessInfo
// ─────────────────────────────────────────────────────────────────────────────

function readProcFile(filePath: string): string | null {
  try { return fs.readFileSync(filePath, 'utf8'); } catch { return null; }
}

/**
 * Build a map from socket inode → owning PID by walking /proc/[pid]/fd/.
 * Falls back to parsing `ss -tlnp` output when /proc traversal yields nothing
 * (e.g. running without root privileges).
 */
export function buildInodeMap(): Map<number, number> {
  const map = new Map<number, number>();

  let pids: string[];
  try {
    pids = fs.readdirSync('/proc').filter(d => /^\d+$/.test(d));
  } catch {
    return map;
  }

  for (const pidStr of pids) {
    const fdDir = `/proc/${pidStr}/fd`;
    let fds: string[];
    try {
      fds = fs.readdirSync(fdDir);
    } catch {
      // Permission denied for processes owned by other users — expected when non-root
      continue;
    }

    for (const fd of fds) {
      try {
        const link = fs.readlinkSync(path.join(fdDir, fd));
        const m = link.match(/^socket:\[(\d+)\]$/);
        if (m) map.set(parseInt(m[1], 10), parseInt(pidStr, 10));
      } catch {
        // fd may have disappeared between readdir and readlink
      }
    }
  }

  return map;
}

// ── UID → username cache ─────────────────────────────────────────────────────

const _uidCache = new Map<number, string | null>();

function uidToUsername(uid: number): string | null {
  if (_uidCache.has(uid)) return _uidCache.get(uid)!;
  try {
    const lines = fs.readFileSync('/etc/passwd', 'utf8').split('\n');
    for (const line of lines) {
      const parts = line.split(':');
      if (parts.length >= 4 && parseInt(parts[2], 10) === uid) {
        _uidCache.set(uid, parts[0]);
        return parts[0];
      }
    }
  } catch { /* ignore */ }
  _uidCache.set(uid, null);
  return null;
}

export function getProcessInfo(pid: number, uid: number): ProcessInfo | null {
  const comm = readProcFile(`/proc/${pid}/comm`)?.trim();
  if (!comm) return null;

  const cmdlineRaw = readProcFile(`/proc/${pid}/cmdline`);
  const cmdline = cmdlineRaw
    ? cmdlineRaw.replace(/\0/g, ' ').trim().slice(0, 200)
    : comm;

  const cgroupRaw = readProcFile(`/proc/${pid}/cgroup`);
  const cgroup = cgroupRaw?.trim().split('\n')[0] ?? null;

  return {
    pid,
    name: comm,
    cmdline,
    user: uidToUsername(uid),
    cgroup,
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Fallback: parse `ss -tlnp` when inode map approach returns nothing
// ─────────────────────────────────────────────────────────────────────────────

export interface SsFallbackEntry {
  pid: number;
  name: string;
}

/**
 * Run `ss -tlnp -H` and extract port → {pid, name} mappings.
 * Output example:  LISTEN  0  511  *:3001  *:*  users:(("node",pid=2545,fd=31))
 */
export function ssLookup(): Map<number, SsFallbackEntry> {
  const map = new Map<number, SsFallbackEntry>();
  try {
    const out = execFileSync('ss', ['-tlnp', '-H'], { encoding: 'utf8', timeout: 3000 });
    for (const line of out.split('\n')) {
      const addrMatch = line.match(/[:\s](\d+)\s+[*\d:.[\]%]+:\*/);
      const userMatch = line.match(/users:\(\("([^"]+)",pid=(\d+)/);
      if (addrMatch && userMatch) {
        const port = parseInt(addrMatch[1], 10);
        map.set(port, { pid: parseInt(userMatch[2], 10), name: userMatch[1] });
      }
    }
  } catch { /* ss not available or failed */ }
  return map;
}

// ─────────────────────────────────────────────────────────────────────────────
// macOS / BSD fallback: use `lsof -i -n -P` to enumerate listening sockets
// ─────────────────────────────────────────────────────────────────────────────

export interface LsofEntry {
  command: string;
  pid: number;
  user: string;
  proto: string;
  localAddress: string;
  localPort: number;
}

function parseLsofOutput(output: string): LsofEntry[] {
  const entries: LsofEntry[] = [];
  for (const line of output.split('\n')) {
    // Format: COMMAND   PID   USER   FD   TYPE  DEVICE SIZE/OFF NODE NAME
    // Example: node  2545  user  31u  IPv4  0x...  0t0  TCP *:3001 (LISTEN)
    const parts = line.trim().split(/\s+/);
    if (parts.length < 9) continue;

    const command = parts[0];
    const pid = parseInt(parts[1], 10);
    const user = parts[2];
    const proto = parts[4];  // IPv4 or IPv6

    const nameField = parts.slice(8).join(' ');
    const nameMatch = nameField.match(/^(TCP|UDP)\s+(\S+):(\d+)(?:\s+\((\w+)\))?/);
    if (!nameMatch) continue;

    const transport = nameMatch[1];
    const addr = nameMatch[2];
    const port = parseInt(nameMatch[3], 10);
    const state = nameMatch[4] || '';

    // Only include LISTEN for TCP, all UDP sockets
    if (transport === 'TCP' && state !== 'LISTEN') continue;

    entries.push({
      command,
      pid: isNaN(pid) ? 0 : pid,
      user,
      proto: `${transport.toLowerCase()}${proto === 'IPv6' ? '6' : ''}` as Proto,
      localAddress: addr === '*' ? '0.0.0.0' : addr,
      localPort: port,
    });
  }
  return entries;
}

export function macosScanSockets(listenOnly = true): RawSocket[] {
  const results: RawSocket[] = [];
  try {
    const out = execFileSync('lsof', ['-i', '-n', '-P'], {
      encoding: 'utf8',
      timeout: 5000,
    });
    const headerEnd = out.indexOf('\n');
    if (headerEnd < 0) return results;
    const entries = parseLsofOutput(out.slice(headerEnd + 1));

    let uidCache: Map<string, number> | null = null;
    const getUid = (username: string): number => {
      if (!uidCache) {
        uidCache = new Map();
        try {
          for (const line of fs.readFileSync('/etc/passwd', 'utf8').split('\n')) {
            const parts = line.split(':');
            if (parts.length >= 3) uidCache.set(parts[0], parseInt(parts[2], 10));
          }
        } catch { /* ignore */ }
      }
      return uidCache.get(username) ?? 0;
    };

    for (const entry of entries) {
      const uid = getUid(entry.user);
      results.push({
        localAddress: entry.localAddress,
        localPort: entry.localPort,
        remoteAddress: '0.0.0.0',
        remotePort: 0,
        state: 'LISTEN',
        uid,
        inode: 0,
        proto: entry.proto,
      });
    }
  } catch { /* lsof not available */ }
  return results;
}

export function macosProcessLookup(port: number): { pid: number; name: string } | null {
  try {
    const out = execFileSync('lsof', ['-i', `:${port}`, '-n', '-P', '-t', '-sTCP:LISTEN'], {
      encoding: 'utf8',
      timeout: 3000,
    });
    const pid = parseInt(out.trim(), 10);
    if (isNaN(pid)) return null;

    const comm = (() => {
      try { return fs.readFileSync(`/proc/${pid}/comm`, 'utf8').trim(); } catch { return 'unknown'; }
    })();
    return { pid, name: comm };
  } catch {
    return null;
  }
}
