import type {
  GuardianDecision,
  GuardianDecisionVerdict,
  GuardianScope,
  GuardianAuditEntry,
  GuardianPolicy,
  PolicyRule,
  RateLimitConfig,
  AlertConfig,
  Alert,
  HealthSnapshot,
} from "./types";
import { mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

let decisionCounter = 0;
let auditCounter = 0;

const decisions = new Map<string, GuardianDecision>();
const auditLog: GuardianAuditEntry[] = [];

const persistedRules: PolicyRule[] = [];
const persistedRateLimitConfigs: RateLimitConfig[] = [];
const persistedAlertConfigs: AlertConfig[] = [];
const persistedAlertHistory: Alert[] = [];
const persistedHealthSnapshots: HealthSnapshot[] = [];

function resolveStateFilePath(): string {
  return resolve(
    process.env.NEXUS_GUARDIAN_STATE_PATH ||
      join(dirname(fileURLToPath(import.meta.url)), "..", "data", "guardian-state.json"),
  );
}

type PersistedGuardianState = {
  version: number;
  counters: {
    decisionCounter: number;
    auditCounter: number;
  };
  decisions: GuardianDecision[];
  auditLog: GuardianAuditEntry[];
  policy: GuardianPolicy;
  rules: PolicyRule[];
  rateLimitConfigs: RateLimitConfig[];
  alertConfigs: AlertConfig[];
  alertHistory: Alert[];
  healthSnapshots: HealthSnapshot[];
  updatedAt: string;
};

const defaultPolicy: GuardianPolicy = {
  policyId: "default",
  description: "Default guardian policy — approve unless explicitly denied",
  version: 1,
  defaultVerdict: "approve",
  enabled: true,
  publicExposureDenyRules: [],
  updatedAt: new Date().toISOString(),
};

let currentPolicy: GuardianPolicy = { ...defaultPolicy };

function generateId(prefix: string): string {
  return `${prefix}-${Date.now()}-${++decisionCounter}`;
}

function persistState(): void {
  const payload: PersistedGuardianState = {
    version: 2,
    counters: { decisionCounter, auditCounter },
    decisions: Array.from(decisions.values()),
    auditLog: [...auditLog],
    policy: { ...currentPolicy, publicExposureDenyRules: [...currentPolicy.publicExposureDenyRules] },
    rules: [...persistedRules],
    rateLimitConfigs: [...persistedRateLimitConfigs],
    alertConfigs: [...persistedAlertConfigs],
    alertHistory: persistedAlertHistory.slice(-200),
    healthSnapshots: [...persistedHealthSnapshots],
    updatedAt: new Date().toISOString(),
  };

  const p = resolveStateFilePath();
  const tempPath = `${p}.tmp`;

  try {
    mkdirSync(dirname(p), { recursive: true });
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, p);
  } catch (error) {
    try {
      rmSync(tempPath, { force: true });
    } catch {
    }
    console.warn(`[nexus-guardian] Failed to persist state: ${(error as Error).message}`);
  }
}

function hydrateStateFromDisk(): void {
  try {
    const raw = readFileSync(resolveStateFilePath(), "utf-8");
    const parsed = JSON.parse(raw) as Partial<PersistedGuardianState>;

    decisions.clear();
    auditLog.length = 0;
    persistedRules.length = 0;
    persistedRateLimitConfigs.length = 0;
    persistedAlertConfigs.length = 0;
    persistedAlertHistory.length = 0;
    persistedHealthSnapshots.length = 0;

    for (const decision of Array.isArray(parsed.decisions) ? parsed.decisions : []) {
      if (decision && typeof decision.scope === "string" && typeof decision.subjectId === "string") {
        decisions.set(decisionKey(decision.scope, decision.subjectId), decision);
      }
    }

    for (const entry of Array.isArray(parsed.auditLog) ? parsed.auditLog : []) {
      auditLog.push(entry);
    }

    decisionCounter = Math.max(0, Number(parsed.counters?.decisionCounter || 0));
    auditCounter = Math.max(0, Number(parsed.counters?.auditCounter || 0));

    const parsedPolicy = parsed.policy;
    if (parsedPolicy && typeof parsedPolicy === "object") {
      currentPolicy = {
        policyId: typeof parsedPolicy.policyId === "string" ? parsedPolicy.policyId : defaultPolicy.policyId,
        description: typeof parsedPolicy.description === "string" ? parsedPolicy.description : defaultPolicy.description,
        version: Number.isFinite(Number(parsedPolicy.version)) ? Number(parsedPolicy.version) : defaultPolicy.version,
        defaultVerdict: parsedPolicy.defaultVerdict || defaultPolicy.defaultVerdict,
        enabled: typeof parsedPolicy.enabled === "boolean" ? parsedPolicy.enabled : defaultPolicy.enabled,
        publicExposureDenyRules: Array.isArray(parsedPolicy.publicExposureDenyRules)
          ? parsedPolicy.publicExposureDenyRules.filter((r): r is string => typeof r === "string")
          : [],
        updatedAt: typeof parsedPolicy.updatedAt === "string" ? parsedPolicy.updatedAt : new Date().toISOString(),
      };
    }

    for (const rule of Array.isArray(parsed.rules) ? parsed.rules : []) {
      if (rule && rule.id && rule.name) persistedRules.push(rule);
    }

    for (const rl of Array.isArray(parsed.rateLimitConfigs) ? parsed.rateLimitConfigs : []) {
      if (rl && rl.id && rl.scope) persistedRateLimitConfigs.push(rl);
    }

    for (const ac of Array.isArray(parsed.alertConfigs) ? parsed.alertConfigs : []) {
      if (ac && ac.id && ac.name) persistedAlertConfigs.push(ac);
    }

    for (const a of Array.isArray(parsed.alertHistory) ? parsed.alertHistory : []) {
      if (a && a.id) persistedAlertHistory.push(a);
    }

    for (const h of Array.isArray(parsed.healthSnapshots) ? parsed.healthSnapshots : []) {
      if (h && h.toolId) persistedHealthSnapshots.push(h);
    }
  } catch {
    persistState();
  }
}

function decisionKey(scope: GuardianScope, subjectId: string): string {
  return `${scope}:${subjectId}`;
}

export function recordDecision(
  scope: GuardianScope,
  subjectId: string,
  verdict: GuardianDecisionVerdict,
  issuedBy = "guardian",
  reason?: string,
  actorRole?: GuardianDecision["actorRole"],
): GuardianDecision {
  const id = generateId("dec");
  const issuedAt = new Date().toISOString();

  const decision: GuardianDecision = { id, scope, subjectId, verdict, reason, issuedAt, issuedBy, actorRole };
  decisions.set(decisionKey(scope, subjectId), decision);

  const auditEntry: GuardianAuditEntry = {
    eventId: `audit-${Date.now()}-${++auditCounter}`,
    decisionId: id,
    scope,
    subjectId,
    verdict,
    timestamp: issuedAt,
    actorRole,
  };
  auditLog.push(auditEntry);

  persistState();

  return decision;
}

export function getDecision(scope: GuardianScope, subjectId: string): GuardianDecision | undefined {
  return decisions.get(decisionKey(scope, subjectId));
}

export function listDecisions(): GuardianDecision[] {
  return Array.from(decisions.values());
}

export function listAuditLog(): GuardianAuditEntry[] {
  return [...auditLog];
}

export function getPolicy(): GuardianPolicy {
  return {
    ...currentPolicy,
    publicExposureDenyRules: [...currentPolicy.publicExposureDenyRules],
  };
}

export function updatePolicy(patch: Partial<Pick<GuardianPolicy, "description" | "defaultVerdict" | "enabled">>): GuardianPolicy {
  currentPolicy = {
    ...currentPolicy,
    description: typeof patch.description === "string" ? patch.description : currentPolicy.description,
    defaultVerdict: patch.defaultVerdict ?? currentPolicy.defaultVerdict,
    enabled: typeof patch.enabled === "boolean" ? patch.enabled : currentPolicy.enabled,
    version: currentPolicy.version + 1,
    updatedAt: new Date().toISOString(),
  };

  persistState();
  return getPolicy();
}

export function setPublicExposureDenyRules(rules: string[]): GuardianPolicy {
  const normalized = Array.from(
    new Set(
      rules
        .map((rule) => rule.trim())
        .filter((rule) => rule.length > 0),
    ),
  );

  currentPolicy = {
    ...currentPolicy,
    publicExposureDenyRules: normalized,
    version: currentPolicy.version + 1,
    updatedAt: new Date().toISOString(),
  };

  persistState();
  return getPolicy();
}

export function evaluatePolicyDecision(scope: GuardianScope, subjectId: string): GuardianDecision | undefined {
  if (!currentPolicy.enabled) return undefined;
  if (scope !== "service") return undefined;

  const normalizedSubject = subjectId.trim().toLowerCase();
  const matchedRule = currentPolicy.publicExposureDenyRules.find((rule) => {
    const normalizedRule = rule.trim().toLowerCase();
    return normalizedRule.length > 0 && normalizedSubject.includes(normalizedRule);
  });

  if (!matchedRule) return undefined;

  const issuedAt = new Date().toISOString();
  return {
    id: `policy-${currentPolicy.version}-${normalizedSubject}`,
    scope,
    subjectId,
    verdict: "deny",
    reason: `Denied by public exposure rule '${matchedRule}' (policy v${currentPolicy.version})`,
    issuedAt,
    issuedBy: "guardian-policy",
  };
}

export function persistRules(rules: PolicyRule[]): void {
  persistedRules.length = 0;
  persistedRules.push(...rules);
  persistState();
}

export function loadPersistedRules(): PolicyRule[] {
  return [...persistedRules];
}

export function persistRateLimitConfigs(configs: RateLimitConfig[]): void {
  persistedRateLimitConfigs.length = 0;
  persistedRateLimitConfigs.push(...configs);
  persistState();
}

export function loadPersistedRateLimitConfigs(): RateLimitConfig[] {
  return [...persistedRateLimitConfigs];
}

export function persistAlertConfigs(configs: AlertConfig[]): void {
  persistedAlertConfigs.length = 0;
  persistedAlertConfigs.push(...configs);
  persistState();
}

export function loadPersistedAlertConfigs(): AlertConfig[] {
  return [...persistedAlertConfigs];
}

export function persistAlertHistory(alerts: Alert[]): void {
  persistedAlertHistory.length = 0;
  persistedAlertHistory.push(...alerts.slice(-200));
  persistState();
}

export function loadPersistedAlertHistory(): Alert[] {
  return [...persistedAlertHistory];
}

export function persistHealthSnapshots(snapshots: HealthSnapshot[]): void {
  persistedHealthSnapshots.length = 0;
  persistedHealthSnapshots.push(...snapshots);
  persistState();
}

export function loadPersistedHealthSnapshots(): HealthSnapshot[] {
  return [...persistedHealthSnapshots];
}

export function clearDecisions(): void {
  decisions.clear();
  auditLog.length = 0;
  decisionCounter = 0;
  auditCounter = 0;
  currentPolicy = {
    ...defaultPolicy,
    publicExposureDenyRules: [...defaultPolicy.publicExposureDenyRules],
    updatedAt: new Date().toISOString(),
  };
  persistedRules.length = 0;
  persistedRateLimitConfigs.length = 0;
  persistedAlertConfigs.length = 0;
  persistedAlertHistory.length = 0;
  persistedHealthSnapshots.length = 0;
  persistState();
}

hydrateStateFromDisk();
