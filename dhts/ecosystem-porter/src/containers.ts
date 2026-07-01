import { execFileSync } from 'node:child_process';
import type { ContainerPort } from './types.js';

// ─────────────────────────────────────────────────────────────────────────────
// Resolve host-mapped ports from running podman / docker containers.
// ─────────────────────────────────────────────────────────────────────────────

function tryExec(cmd: string, args: string[]): string | null {
  try {
    return execFileSync(cmd, args, {
      timeout: 5000,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
    });
  } catch {
    return null;
  }
}

// ── Podman JSON format ───────────────────────────────────────────────────────

interface PodmanPort {
  hostIP?: string;
  containerPort: number;
  hostPort: number;
  range?: string;
  protocol: string;
}

interface PodmanContainer {
  Id: string;
  Names: string | string[];
  Image: string;
  State: string;
  Ports?: PodmanPort[] | null;
}

// ── Docker JSON format (one JSON object per line, via `--format {{json .}}`) ─

interface DockerPort {
  IP?: string;
  PrivatePort: number;
  PublicPort?: number;
  Type: string;
}

interface DockerContainer {
  ID: string;
  Names: string;
  Image: string;
  Status: string;
  Ports: DockerPort[] | string; // docker inspect can return string
}

// ─────────────────────────────────────────────────────────────────────────────

function parsePodmanOutput(raw: string): Map<number, ContainerPort> {
  const map = new Map<number, ContainerPort>();
  let containers: PodmanContainer[];
  try {
    containers = JSON.parse(raw) as PodmanContainer[];
  } catch {
    return map;
  }

  for (const c of containers) {
    const name = Array.isArray(c.Names)
      ? (c.Names[0] ?? 'unknown').replace(/^\//, '')
      : String(c.Names ?? 'unknown').replace(/^\//, '');

    for (const p of c.Ports ?? []) {
      if (p.hostPort > 0) {
        map.set(p.hostPort, {
          containerName: name,
          containerId: (c.Id ?? '').slice(0, 12),
          image: c.Image ?? '',
          containerPort: p.containerPort,
          hostPort: p.hostPort,
          protocol: p.protocol ?? 'tcp',
        });
      }
    }
  }
  return map;
}

function parseDockerOutput(raw: string): Map<number, ContainerPort> {
  const map = new Map<number, ContainerPort>();
  // docker ps --format '{{json .}}' emits one JSON object per line
  const lines = raw.trim().split('\n').filter(Boolean);
  for (const line of lines) {
    let c: DockerContainer;
    try { c = JSON.parse(line) as DockerContainer; } catch { continue; }

    const name = String(c.Names ?? '').replace(/^\//, '') || 'unknown';
    const ports: DockerPort[] = Array.isArray(c.Ports) ? c.Ports : [];

    for (const p of ports) {
      if (p.PublicPort) {
        map.set(p.PublicPort, {
          containerName: name,
          containerId: (c.ID ?? '').slice(0, 12),
          image: c.Image ?? '',
          containerPort: p.PrivatePort,
          hostPort: p.PublicPort,
          protocol: p.Type ?? 'tcp',
        });
      }
    }
  }
  return map;
}

/**
 * Query podman (primary) and docker (fallback) for running containers with
 * published ports.  Returns a map from host-port → ContainerPort metadata.
 */
export function getContainerPorts(): Map<number, ContainerPort> {
  // Podman returns a JSON array directly
  const podmanRaw = tryExec('podman', ['ps', '--format', 'json']);
  if (podmanRaw?.trim().startsWith('[')) {
    const result = parsePodmanOutput(podmanRaw);
    if (result.size > 0) return result;
  }

  // Docker: one JSON object per line
  const dockerRaw = tryExec('docker', ['ps', '--format', '{{json .}}']);
  if (dockerRaw) {
    const result = parseDockerOutput(dockerRaw);
    if (result.size > 0) return result;
  }

  return new Map();
}
