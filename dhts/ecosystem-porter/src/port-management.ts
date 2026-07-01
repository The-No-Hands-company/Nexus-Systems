import { findFreePorts, runScan } from './scan.js';
import type {
	OutboundIntelligenceStub,
	PortConflictItem,
	PortConflictReport,
	PortReconfigurationApplyResult,
	PortReconfigurationPlan,
	PortToolCapability,
} from './types.js';

const DEFAULT_REMAP_RANGE = { from: 2000, to: 20000 };

export function getPortToolCapabilities(): PortToolCapability[] {
	return [
		{
			id: 'inventory-live-scan',
			name: 'Live listening port inventory',
			status: 'implemented',
			description: 'Scans /proc and container runtime mappings to show active listening sockets.',
			nextSteps: [],
		},
		{
			id: 'free-port-discovery',
			name: 'Free port discovery',
			status: 'implemented',
			description: 'Returns currently free ports in a requested range.',
			nextSteps: [],
		},
		{
			id: 'ownership-attribution',
			name: 'Port ownership attribution',
			status: 'implemented',
			description: 'Maps listening ports to process, user and known service metadata where possible.',
			nextSteps: [],
		},
		{
			id: 'http-intelligence',
			name: 'HTTP behavior and embeddability',
			status: 'implemented',
			description: 'Optional HTTP probe for status/title/security headers and iframe embeddability.',
			nextSteps: [],
		},
		{
			id: 'reconfiguration-planner',
			name: 'Safe port reconfiguration planner',
			status: 'stub',
			description: 'Builds a dry-run migration plan to move a service from one port to another.',
			nextSteps: [
				'Add per-framework adapters (Node, Docker Compose, Podman, Nginx, systemd).',
				'Generate editable patch suggestions for config files in dependent repos.',
				'Add pre-flight checks for privileges and restart impact.',
			],
		},
		{
			id: 'port-reservation-registry',
			name: 'Reservation and policy registry',
			status: 'stub',
			description: 'Reserve ranges/ports for ecosystem services to prevent collisions.',
			nextSteps: [
				'Define machine-local reservation file format.',
				'Enforce reservation checks during scan and planning.',
			],
		},
		{
			id: 'outbound-intelligence',
			name: 'Outbound connection intelligence',
			status: 'stub',
			description: 'Track outbound client connections and destination analysis.',
			nextSteps: [
				'Add parsers for ss/netstat conn states.',
				'Correlate destinations to owning processes and tags.',
			],
		},
	];
}

export async function findPortConflicts(targetPorts: number[]): Promise<PortConflictReport> {
	const uniqueTargets = Array.from(new Set(targetPorts.filter((p) => p >= 1 && p <= 65535))).sort((a, b) => a - b);
	const scan = await runScan({ listenOnly: true, probe: false });
	const byPort = new Map(scan.ports.map((entry) => [entry.port, entry]));

	const taken: PortConflictItem[] = [];
	const free: number[] = [];

	for (const port of uniqueTargets) {
		const entry = byPort.get(port);
		if (!entry) {
			free.push(port);
			continue;
		}

		taken.push({
			port,
			taken: true,
			owner: entry.process,
			known: entry.known,
			protocols: entry.protocols,
		});
	}

	return {
		scannedAt: scan.scannedAt,
		totalTargets: uniqueTargets.length,
		taken,
		free,
	};
}

export interface ReconfigurationPlanOptions {
	fromPort: number;
	toPort?: number;
	rangeFrom?: number;
	rangeTo?: number;
}

export async function createPortReconfigurationPlan(
	options: ReconfigurationPlanOptions,
): Promise<PortReconfigurationPlan> {
	const fromPort = clampPort(options.fromPort);
	const explicitTo = options.toPort !== undefined ? clampPort(options.toPort) : undefined;
	const rangeFrom = clampPort(options.rangeFrom ?? DEFAULT_REMAP_RANGE.from);
	const rangeTo = clampPort(options.rangeTo ?? DEFAULT_REMAP_RANGE.to);

	const [scan, conflicts] = await Promise.all([
		runScan({ listenOnly: true, probe: false }),
		findPortConflicts(explicitTo !== undefined ? [fromPort, explicitTo] : [fromPort]),
	]);
	const current = scan.ports.find((p) => p.port === fromPort) ?? null;

	let candidatePort: number | null = explicitTo ?? null;
	const warnings: string[] = [];

	if (explicitTo !== undefined && conflicts.taken.some((c) => c.port === explicitTo)) {
		warnings.push(`Requested target port ${explicitTo} is already in use.`);
		candidatePort = null;
	}

	if (candidatePort === null) {
		const from = Math.min(rangeFrom, rangeTo);
		const to = Math.max(rangeFrom, rangeTo);
		const free = await findFreePorts(from, to);
		candidatePort = free[0] ?? null;
		if (candidatePort === null) {
			warnings.push(`No free candidate port found in range ${from}-${to}.`);
		}
	}

	if (!current) {
		warnings.push(`Source port ${fromPort} is not currently listening.`);
	}

	return {
		createdAt: new Date().toISOString(),
		fromPort,
		current,
		candidatePort,
		mode: 'dry-run',
		conflicts,
		safetyChecks: [
			'Confirm source service ownership before changing configuration.',
			'Update firewall rules and reverse proxies together with service config.',
			'Restart or reload service and verify health endpoint on the new port.',
			'Keep rollback instructions prepared before applying any changes.',
		],
		steps: [
			'Identify config files that define the source port.',
			'Patch service config to the target port and adjust dependent mappings.',
			'Restart service process/container and verify that source port closes.',
			'Run Nexus Porter scan and conflict check to validate the migration.',
			'Update ecosystem documentation and reserved-port registry (future feature).',
		],
		warnings,
	};
}

export function applyPortReconfigurationPlanStub(
	plan: PortReconfigurationPlan,
): PortReconfigurationApplyResult {
	return {
		ok: false,
		mode: 'stub',
		message: `Apply is intentionally not implemented yet. Dry-run plan created for ${plan.fromPort}${
			plan.candidatePort !== null ? ` -> ${plan.candidatePort}` : ''
		}`,
		manualSteps: [
			'Review the generated plan and perform changes in your service configuration manually.',
			'Restart impacted services and rerun scan/plan endpoints to validate the result.',
			'Implement adapters before enabling automatic writes in this repository.',
		],
	};
}

export function getOutboundIntelligenceStub(): OutboundIntelligenceStub {
	return {
		status: 'stub',
		message: 'Outbound intelligence is not implemented yet.',
		suggestedSources: [
			'ss -tunap for live outbound socket state',
			'/proc/net/tcp + /proc/net/udp with remote address correlation',
			'Conntrack or eBPF stream for short-lived connection visibility',
		],
		suggestedFutureEndpoints: [
			'GET /outbound',
			'GET /outbound/:pid',
			'GET /outbound/summary?window=60s',
		],
	};
}

function clampPort(port: number): number {
	if (!Number.isFinite(port)) return 1;
	if (port < 1) return 1;
	if (port > 65535) return 65535;
	return Math.floor(port);
}
