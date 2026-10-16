import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { buildSystemsApiRegistrationPayload } from "./contracts";
import { startNexusGuardianCloudRegistrationHeartbeat } from "./cloud";
import {
  recordDecision,
  getDecision,
  listDecisions,
  listAuditLog,
  getPolicy,
  updatePolicy,
  setPublicExposureDenyRules,
  evaluatePolicyDecision,
  clearDecisions,
  persistRules as persistRulesToState,
  loadPersistedRules,
  persistRateLimitConfigs as persistRlToState,
  loadPersistedRateLimitConfigs,
  persistAlertConfigs as persistAcToState,
  loadPersistedAlertConfigs,
  persistAlertHistory as persistAhToState,
  loadPersistedAlertHistory,
  loadPersistedHealthSnapshots,
  persistHealthSnapshots,
} from "./state";
import {
  upsertRule,
  getRule,
  listRules,
  deleteRule,
  setRuleEnabled,
  evaluateRequest,
  seedDefaultRules,
  importRules,
  clearRules,
} from "./rules";
import {
  upsertRateLimitConfig,
  getRateLimitConfig,
  listRateLimitConfigs,
  deleteRateLimitConfig,
  setRateLimitConfigEnabled,
  checkRateLimit,
  recordRequest,
  getRateLimitStatus,
  seedDefaultRateLimits,
  clearRateLimits,
} from "./rate-limiter";
import {
  registerToolForMonitoring,
  heartbeatTool,
  getHealthSnapshot,
  listHealthSnapshots,
  deregisterTool,
  startMonitoring,
  stopMonitoring,
  stopAllMonitoring,
  getUnhealthyTools,
} from "./health-monitor";
import {
  upsertAlertConfig,
  getAlertConfig,
  listAlertConfigs,
  deleteAlertConfig,
  setAlertConfigEnabled,
  dispatchAlert,
  listAlerts,
  getAlert,
  seedDefaultAlertConfigs,
  clearAlertConfigs,
  clearAlertHistory,
} from "./alerts";
import {
  scanInput,
  scanMultiple,
  getRecentMatches,
  getMatchesForSource,
  getMatchStats,
  listThreatPatterns,
  upsertThreatPattern,
  deleteThreatPattern,
} from "./threat-detector";
import {
  recordRequest as ipRecordRequest,
  recordBlock as ipRecordBlock,
  addScore as ipAddScore,
  isBlacklisted,
  getIpRecord,
  listIpRecords,
  getBlacklistedIps,
  unblacklist as ipUnblacklist,
  applyDecay as ipApplyDecay,
  getStats as ipGetStats,
} from "./ip-reputation";
import {
  recordRate,
  getRateTracker,
  listRateTrackers,
  getRateAnomalies,
} from "./rate-anomaly";
import type {
  GuardianScope,
  GuardianDecisionVerdict,
  GuardianDecision,
  ThreatResponseRequest,
  ThreatResponseResult,
  AlertLevel,
  AlertChannel,
  HealthStatus,
  PolicyRule,
} from "./types";

const ALL_SCOPES: GuardianScope[] = [
  "service", "user", "agent", "network", "resource",
  "exposure", "domain", "runtime",
];
const ALL_VERDICTS: GuardianDecisionVerdict[] = ["approve", "deny", "suspend", "quarantine"];
const ALL_LEVELS: AlertLevel[] = ["info", "warning", "error", "critical"];
const ALL_CHANNELS: AlertChannel[] = ["webhook", "audit", "log"];

function jsonResponse(payload: unknown, init?: ResponseInit): Response {
  return new Response(JSON.stringify(payload, null, 2), {
    ...init,
    headers: {
      "content-type": "application/json; charset=utf-8",
      ...(init?.headers || {}),
    },
  });
}

async function parseBody(request: Request): Promise<Record<string, unknown>> {
  try {
    const parsed = await request.json();
    return typeof parsed === "object" && parsed !== null ? (parsed as Record<string, unknown>) : {};
  } catch {
    return {};
  }
}

function isValidScope(value: string): value is GuardianScope {
  return (ALL_SCOPES as string[]).includes(value);
}

function isValidVerdict(value: string): value is GuardianDecisionVerdict {
  return (ALL_VERDICTS as string[]).includes(value);
}

export function createGuardianServer() {
  const port = Number(process.env.PORT || "4320");
  const baseUrl = process.env.NEXUS_GUARDIAN_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled =
    (process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  seedDefaultRules();
  seedDefaultRateLimits();
  seedDefaultAlertConfigs();

  const loadedRules = loadPersistedRules();
  if (loadedRules.length > 0) {
    clearRules();
    importRules(loadedRules);
    seedDefaultRules();
  }

  const loadedRl = loadPersistedRateLimitConfigs();
  if (loadedRl.length > 0) {
    clearRateLimits();
    for (const rl of loadedRl) {
      upsertRateLimitConfig(rl);
    }
    seedDefaultRateLimits();
  }

  const loadedAc = loadPersistedAlertConfigs();
  if (loadedAc.length > 0) {
    clearAlertConfigs();
    for (const ac of loadedAc) {
      upsertAlertConfig(ac);
    }
    seedDefaultAlertConfigs();
  }

  const loadedHealth = loadPersistedHealthSnapshots();
  for (const h of loadedHealth) {
    registerToolForMonitoring(h);
  }

  const persistInterval = setInterval(() => {
    persistRulesToState(listRules());
    persistRlToState(listRateLimitConfigs());
    persistAcToState(listAlertConfigs());
    persistAhToState(listAlerts(200));
    persistHealthSnapshots(listHealthSnapshots());
  }, 10000);

  if (typeof persistInterval.unref === "function") {
    persistInterval.unref();
  }

  const ipDecayTimer = setInterval(() => { ipApplyDecay(); }, 60000);
  if (typeof ipDecayTimer.unref === "function") ipDecayTimer.unref();

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-guardian",
  name: "Nexus Guardian",
  description: "Central safety, security, and policy enforcement engine",
  port: 4320,
  capabilities: ["policy-enforcement","threat-detection","rule-engine","rate-limiting","health-monitoring","alert-dispatch"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    // Loopback unless told otherwise — Bun.serve binds every interface by
    // default; infrastructure services should only be reachable locally.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({
          status: "ok",
          service: "nexus-guardian",
          timestamp: new Date().toISOString(),
        });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/readiness") {
        return jsonResponse({
          status: "ready",
          contracts: {
            decisions: "/api/v1/guardian/decisions",
            audit: "/api/v1/guardian/audit",
            policy: "/api/v1/guardian/policy",
            policyPublicExposureDenyRules: "/api/v1/guardian/policy/public-exposure/deny",
            decide: "/api/v1/guardian/:scope/:subjectId/:verdict",
            rules: "/api/v1/guardian/rules",
            evaluate: "/api/v1/guardian/evaluate",
            rateLimits: "/api/v1/guardian/rate-limits",
            rateLimitStatus: "/api/v1/guardian/rate-limits/:toolId",
            healthSnapshot: "/api/v1/guardian/health/snapshot",
            healthMonitor: "/api/v1/guardian/health/monitor",
            alerts: "/api/v1/guardian/alerts",
            alertConfigs: "/api/v1/guardian/alerts/config",
            threatRespond: "/api/v1/guardian/threat/respond",
            threatDetect: "/api/v1/guardian/detect",
            ipReputation: "/api/v1/guardian/ips",
            rateAnomaly: "/api/v1/guardian/rate",
          },
          capabilities: {
            scopes: ALL_SCOPES,
            verdicts: ALL_VERDICTS,
            ruleCount: listRules().length,
            rateLimitCount: listRateLimitConfigs().length,
            alertConfigCount: listAlertConfigs().length,
            monitoredToolsCount: listHealthSnapshots().length,
          },
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        });
      }

      // ── Decisions ──
      if (request.method === "GET" && path === "/api/v1/guardian/decisions") {
        return jsonResponse({ status: "ok", decisions: listDecisions() });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/audit") {
        return jsonResponse({ status: "ok", auditLog: listAuditLog() });
      }

      // ── Policy ──
      if (request.method === "GET" && path === "/api/v1/guardian/policy") {
        return jsonResponse({ status: "ok", policy: getPolicy() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/policy") {
        const body = await parseBody(request);
        const defaultVerdict = typeof body.defaultVerdict === "string" && isValidVerdict(body.defaultVerdict)
          ? body.defaultVerdict
          : undefined;
        const description = typeof body.description === "string" ? body.description : undefined;
        const enabled = typeof body.enabled === "boolean" ? body.enabled : undefined;
        const policy = updatePolicy({ defaultVerdict, description, enabled });
        return jsonResponse({ status: "ok", policy });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/policy/public-exposure/deny") {
        const policy = getPolicy();
        return jsonResponse({ status: "ok", rules: policy.publicExposureDenyRules, version: policy.version });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/policy/public-exposure/deny") {
        const body = await parseBody(request);
        const incomingRules = Array.isArray(body.rules) ? body.rules : [];
        const rules = incomingRules.filter((r): r is string => typeof r === "string");
        const policy = setPublicExposureDenyRules(rules);
        return jsonResponse({ status: "ok", policy });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/contracts/registration") {
        return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(baseUrl) });
      }

      // ── Rule Engine ──
      if (request.method === "GET" && path === "/api/v1/guardian/rules") {
        return jsonResponse({ status: "ok", rules: listRules() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/rules") {
        const body = await parseBody(request);
        const name = typeof body.name === "string" ? body.name.trim() : "";
        if (!name) return jsonResponse({ error: "name is required" }, { status: 400 });

        const rule = upsertRule({
          id: typeof body.id === "string" ? body.id : undefined,
          name,
          description: typeof body.description === "string" ? body.description : "",
          priority: typeof body.priority === "number" ? body.priority : 0,
          scopes: Array.isArray(body.scopes) ? body.scopes.filter((s): s is GuardianScope => typeof s === "string" && isValidScope(s)) : [],
          conditions: Array.isArray(body.conditions) ? body.conditions : [],
          action: typeof body.action === "string" ? body.action as PolicyRule["action"] : "log",
          metadata: typeof body.metadata === "object" && body.metadata !== null ? body.metadata as Record<string, string> : undefined,
        });

        if (typeof body.enabled === "boolean") {
          setRuleEnabled(rule.id, body.enabled);
        }

        persistRulesToState(listRules());
        return jsonResponse({ status: "ok", rule }, { status: 201 });
      }

      // POST /api/v1/guardian/evaluate
      if (request.method === "POST" && path === "/api/v1/guardian/evaluate") {
        const body = await parseBody(request);
        const scope = typeof body.scope === "string" && isValidScope(body.scope) ? body.scope : undefined;
        const subjectId = typeof body.subjectId === "string" ? body.subjectId : "";
        if (!scope || !subjectId) {
          return jsonResponse({ error: "scope and subjectId are required" }, { status: 400 });
        }

        const context = typeof body.context === "object" && body.context !== null
          ? body.context as Record<string, string>
          : {};

        const policy = getPolicy();
        const result = evaluateRequest(
          { scope, subjectId, context },
          policy.defaultVerdict,
        );

        const decision = recordDecision(scope, subjectId, result.verdict, "rule-engine", result.reason, "system");
        const rateStatus = checkRateLimit(subjectId, scope);
        recordRequest(subjectId, scope);

        return jsonResponse({
          status: "ok",
          evaluation: result,
          decision,
          rateLimit: rateStatus,
        });
      }

      // ── Rules by ID ──
      const getRuleMatch = path.match(/^\/api\/v1\/guardian\/rules\/([^/]+)$/);
      if (request.method === "GET" && getRuleMatch) {
        const [, ruleId] = getRuleMatch;
        const rule = getRule(decodeURIComponent(ruleId));
        if (!rule) return jsonResponse({ error: "Rule not found" }, { status: 404 });
        return jsonResponse({ status: "ok", rule });
      }

      if (request.method === "DELETE" && getRuleMatch) {
        const [, ruleId] = getRuleMatch;
        const deleted = deleteRule(decodeURIComponent(ruleId));
        if (!deleted) return jsonResponse({ error: "Rule not found" }, { status: 404 });
        persistRulesToState(listRules());
        return jsonResponse({ status: "ok", deleted: true });
      }

      const toggleRuleMatch = path.match(/^\/api\/v1\/guardian\/rules\/([^/]+)\/(enable|disable)$/);
      if (request.method === "POST" && toggleRuleMatch) {
        const [, ruleId, action] = toggleRuleMatch;
        const rule = setRuleEnabled(decodeURIComponent(ruleId), action === "enable");
        if (!rule) return jsonResponse({ error: "Rule not found" }, { status: 404 });
        persistRulesToState(listRules());
        return jsonResponse({ status: "ok", rule });
      }

      // ── Rate Limits ──
      if (request.method === "GET" && path === "/api/v1/guardian/rate-limits") {
        return jsonResponse({ status: "ok", rateLimits: listRateLimitConfigs() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/rate-limits") {
        const body = await parseBody(request);
        const scope = typeof body.scope === "string" && isValidScope(body.scope) ? body.scope : undefined;
        if (!scope) return jsonResponse({ error: "Valid scope is required" }, { status: 400 });

        const config = upsertRateLimitConfig({
          id: typeof body.id === "string" ? body.id : undefined,
          scope,
          maxRequests: typeof body.maxRequests === "number" ? body.maxRequests : 10,
          windowMs: typeof body.windowMs === "number" ? body.windowMs : 60000,
          description: typeof body.description === "string" ? body.description : "",
        });

        if (typeof body.enabled === "boolean") {
          setRateLimitConfigEnabled(config.id, body.enabled);
        }

        persistRlToState(listRateLimitConfigs());
        return jsonResponse({ status: "ok", config }, { status: 201 });
      }

      // GET /api/v1/guardian/rate-limits/:toolId
      const rlStatusMatch = path.match(/^\/api\/v1\/guardian\/rate-limits\/([^/]+)$/);
      if (request.method === "GET" && rlStatusMatch) {
        const [, toolId] = rlStatusMatch;
        const status = getRateLimitStatus(decodeURIComponent(toolId));
        return jsonResponse({ status: "ok", rateLimits: status });
      }

      const getRlMatch = path.match(/^\/api\/v1\/guardian\/rate-limits\/config\/([^/]+)$/);
      if (request.method === "DELETE" && getRlMatch) {
        const [, configId] = getRlMatch;
        const deleted = deleteRateLimitConfig(decodeURIComponent(configId));
        if (!deleted) return jsonResponse({ error: "Rate limit config not found" }, { status: 404 });
        persistRlToState(listRateLimitConfigs());
        return jsonResponse({ status: "ok", deleted: true });
      }

      // ── Health Monitoring ──
      if (request.method === "GET" && path === "/api/v1/guardian/health/snapshot") {
        return jsonResponse({ status: "ok", snapshots: listHealthSnapshots() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/health/heartbeat") {
        const body = await parseBody(request);
        const toolId = typeof body.toolId === "string" ? body.toolId.trim() : "";
        const toolName = typeof body.toolName === "string" ? body.toolName.trim() : toolId;
        const upstreamUrl = typeof body.upstreamUrl === "string" ? body.upstreamUrl.trim() : "";
        const health = typeof body.health === "string" ? body.health as HealthStatus : "healthy";

        if (!toolId) return jsonResponse({ error: "toolId is required" }, { status: 400 });

        const snapshot = heartbeatTool(toolId, toolName, upstreamUrl, health);
        persistHealthSnapshots(listHealthSnapshots());
        return jsonResponse({ status: "ok", snapshot });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/health/monitor") {
        const body = await parseBody(request);
        const toolId = typeof body.toolId === "string" ? body.toolId.trim() : "";
        const toolName = typeof body.toolName === "string" ? body.toolName.trim() : toolId;
        const upstreamUrl = typeof body.upstreamUrl === "string" ? body.upstreamUrl.trim() : "";
        const intervalMs = typeof body.intervalMs === "number" ? body.intervalMs : 30000;

        if (!toolId || !upstreamUrl) {
          return jsonResponse({ error: "toolId and upstreamUrl are required" }, { status: 400 });
        }

        const snapshot = startMonitoring(toolId, toolName, upstreamUrl, intervalMs);
        return jsonResponse({ status: "ok", snapshot }, { status: 201 });
      }

      const stopMonitorMatch = path.match(/^\/api\/v1\/guardian\/health\/monitor\/([^/]+)\/stop$/);
      if (request.method === "POST" && stopMonitorMatch) {
        const [, toolId] = stopMonitorMatch;
        stopMonitoring(decodeURIComponent(toolId));
        return jsonResponse({ status: "ok", stopped: true });
      }

      const deregisterHealthMatch = path.match(/^\/api\/v1\/guardian\/health\/monitor\/([^/]+)$/);
      if (request.method === "DELETE" && deregisterHealthMatch) {
        const [, toolId] = deregisterHealthMatch;
        deregisterTool(decodeURIComponent(toolId));
        return jsonResponse({ status: "ok", deregistered: true });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/health/unhealthy") {
        return jsonResponse({ status: "ok", unhealthy: getUnhealthyTools() });
      }

      // ── Alerts ──
      if (request.method === "GET" && path === "/api/v1/guardian/alerts") {
        const limit = Number(url.searchParams.get("limit") || "50");
        return jsonResponse({ status: "ok", alerts: listAlerts(Math.min(limit, 500)) });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/alerts") {
        const body = await parseBody(request);
        const level = typeof body.level === "string" && (ALL_LEVELS as string[]).includes(body.level)
          ? body.level as AlertLevel
          : "info";
        const eventType = typeof body.eventType === "string" ? body.eventType : "custom";
        const message = typeof body.message === "string" ? body.message : "";
        if (!message) return jsonResponse({ error: "message is required" }, { status: 400 });

        const detail = typeof body.detail === "object" && body.detail !== null
          ? body.detail as Record<string, unknown>
          : undefined;

        const alert = await dispatchAlert({ level, eventType, message, detail });
        persistAhToState(listAlerts(200));
        return jsonResponse({ status: "ok", alert }, { status: 201 });
      }

      // Alert configs
      if (request.method === "GET" && path === "/api/v1/guardian/alerts/config") {
        return jsonResponse({ status: "ok", configs: listAlertConfigs() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/alerts/config") {
        const body = await parseBody(request);
        const name = typeof body.name === "string" ? body.name.trim() : "";
        if (!name) return jsonResponse({ error: "name is required" }, { status: 400 });

        const channel = typeof body.channel === "string" && (ALL_CHANNELS as string[]).includes(body.channel)
          ? body.channel as AlertChannel
          : "log";

        const config = upsertAlertConfig({
          id: typeof body.id === "string" ? body.id : undefined,
          name,
          channel,
          webhookUrl: typeof body.webhookUrl === "string" ? body.webhookUrl : undefined,
          levels: Array.isArray(body.levels)
            ? body.levels.filter((l): l is AlertLevel => typeof l === "string" && (ALL_LEVELS as string[]).includes(l))
            : ["info"],
          eventTypes: Array.isArray(body.eventTypes)
            ? body.eventTypes.filter((e): e is string => typeof e === "string")
            : [],
        });

        if (typeof body.enabled === "boolean") {
          setAlertConfigEnabled(config.id, body.enabled);
        }

        persistAcToState(listAlertConfigs());
        return jsonResponse({ status: "ok", config }, { status: 201 });
      }

      const getAcMatch = path.match(/^\/api\/v1\/guardian\/alerts\/config\/([^/]+)$/);
      if (request.method === "DELETE" && getAcMatch) {
        const [, configId] = getAcMatch;
        const deleted = deleteAlertConfig(decodeURIComponent(configId));
        if (!deleted) return jsonResponse({ error: "Alert config not found" }, { status: 404 });
        persistAcToState(listAlertConfigs());
        return jsonResponse({ status: "ok", deleted: true });
      }

      const toggleAcMatch = path.match(/^\/api\/v1\/guardian\/alerts\/config\/([^/]+)\/(enable|disable)$/);
      if (request.method === "POST" && toggleAcMatch) {
        const [, configId, action] = toggleAcMatch;
        const config = setAlertConfigEnabled(decodeURIComponent(configId), action === "enable");
        if (!config) return jsonResponse({ error: "Alert config not found" }, { status: 404 });
        persistAcToState(listAlertConfigs());
        return jsonResponse({ status: "ok", config });
      }

      // ── Threat Response ──
      if (request.method === "POST" && path === "/api/v1/guardian/threat/respond") {
        const body = await parseBody(request) as Partial<ThreatResponseRequest>;
        const toolId = typeof body.toolId === "string" ? body.toolId.trim() : "";
        const threatType = typeof body.threatType === "string" ? body.threatType : "unknown";
        const severity = typeof body.severity === "string" &&
          ["low", "medium", "high", "critical"].includes(body.severity)
          ? body.severity as ThreatResponseRequest["severity"]
          : "medium";
        const description = typeof body.description === "string" ? body.description : "Threat detected";

        if (!toolId) return jsonResponse({ error: "toolId is required" }, { status: 400 });

        const context: Record<string, string> = {
          threatType,
          threatSeverity: severity,
          ...(body.context || {}),
        };

        const policy = getPolicy();
        const result = evaluateRequest(
          { scope: "agent", subjectId: toolId, context },
          policy.defaultVerdict,
        );

        const severityActions: Record<string, ThreatResponseResult["recommendedAction"]> = {
          critical: "block",
          high: "alert",
          medium: "throttle",
          low: "log",
        };

        let recommendedAction: ThreatResponseResult["recommendedAction"] =
          severityActions[severity] || "log";

        let reason = `Severity-based action '${recommendedAction}' for '${severity}' severity`;

        if (result.verdict === "deny") {
          recommendedAction = "block";
          reason = `Guardian policy denied: ${result.reason}`;
        } else if (result.verdict === "suspend" || result.verdict === "quarantine") {
          recommendedAction = "isolate";
          reason = `Guardian policy ${result.verdict}: ${result.reason}`;
        }

        const decision = recordDecision("agent", toolId, result.verdict, "threat-response", reason, "system");

        await dispatchAlert({
          level: severity === "critical" ? "critical" : severity === "high" ? "error" : "warning",
          eventType: "threat_detected",
          message: `Threat response for ${toolId}: ${recommendedAction} — ${description}`,
          detail: {
            toolId,
            threatType,
            severity,
            description,
            recommendedAction,
            decisionId: decision.id,
          },
        });

        const response: ThreatResponseResult = {
          recommendedAction,
          reason,
          decisionId: decision.id,
        };

        return jsonResponse({ status: "ok", response });
      }

      // ── Decision by scope/subject (POST and GET) ──
      const decisionMatch = path.match(/^\/api\/v1\/guardian\/([^/]+)\/([^/]+)\/([^/]+)$/);
      if (request.method === "POST" && decisionMatch) {
        const [, rawScope, subjectId, rawVerdict] = decisionMatch;

        if (!isValidScope(rawScope)) {
          return jsonResponse(
            { error: `Invalid scope '${rawScope}'. Valid scopes: ${ALL_SCOPES.join(", ")}` },
            { status: 400 },
          );
        }

        if (!isValidVerdict(rawVerdict)) {
          return jsonResponse(
            { error: `Invalid verdict '${rawVerdict}'. Valid verdicts: ${ALL_VERDICTS.join(", ")}` },
            { status: 400 },
          );
        }

        const body = await parseBody(request);
        const reason = typeof body.reason === "string" ? body.reason : undefined;
        const issuedBy = typeof body.issuedBy === "string" ? body.issuedBy : "guardian-api";
        const actorRole = typeof body.actorRole === "string" &&
          ["operator", "service", "peer", "system"].includes(body.actorRole)
          ? body.actorRole as GuardianDecision["actorRole"]
          : undefined;

        const decision = recordDecision(rawScope, decodeURIComponent(subjectId), rawVerdict, issuedBy, reason, actorRole);
        return jsonResponse({ status: "ok", decision }, { status: 201 });
      }

      const lookupMatch = path.match(/^\/api\/v1\/guardian\/([^/]+)\/([^/]+)$/);
      if (request.method === "GET" && lookupMatch) {
        const [, rawScope, subjectId] = lookupMatch;

        if (!isValidScope(rawScope)) {
          return jsonResponse({ error: `Invalid scope '${rawScope}'` }, { status: 400 });
        }

        const decision = getDecision(rawScope, decodeURIComponent(subjectId));
        const policyDecision = decision ? undefined : evaluatePolicyDecision(rawScope, decodeURIComponent(subjectId));
        const resolvedDecision = decision || policyDecision;

        if (!resolvedDecision) {
          return jsonResponse({ error: "No decision found" }, { status: 404 });
        }

        return jsonResponse({ status: "ok", decision: resolvedDecision, source: decision ? "decision" : "policy" });
      }

      // ── Threat Detection ──
      if (request.method === "POST" && path === "/api/v1/guardian/detect") {
        const body = await parseBody(request);
        const field = typeof body.field === "string" ? body.field : "body";
        const value = typeof body.value === "string" ? body.value : "";
        const source = typeof body.source === "string" ? body.source : "api";
        if (!value) return jsonResponse({ error: "value is required" }, { status: 400 });

        const matches = scanInput({ field, value, source });

        for (const match of matches) {
          const ip = typeof body.ip === "string" ? body.ip : source;
          if (match.autoResponse === "block") {
            ipRecordBlock(ip, match.patternName);
          } else {
            ipAddScore(ip, match.severity === "critical" ? 20 : match.severity === "high" ? 10 : 5, match.patternName);
          }

          await dispatchAlert({
            level: match.severity === "critical" ? "critical" : match.severity === "high" ? "error" : "warning",
            eventType: "threat_detected",
            message: `Threat detected: ${match.patternName} (${match.category}) from ${source}`,
            detail: {
              patternId: match.patternId,
              patternName: match.patternName,
              category: match.category,
              severity: match.severity,
              field: match.field,
              matchedValue: match.matchedValue,
              source: match.source,
              autoResponse: match.autoResponse,
            },
          });
        }

        return jsonResponse({ status: "ok", matches, matchCount: matches.length });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/detect/batch") {
        const body = await parseBody(request);
        const inputs = Array.isArray(body.inputs) ? body.inputs as Array<{ field: string; value: string; source: string }> : [];
        const matches = scanMultiple(inputs);
        return jsonResponse({ status: "ok", matches, matchCount: matches.length });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/detect/matches") {
        const source = url.searchParams.get("source");
        const matches = source ? getMatchesForSource(source) : getRecentMatches();
        return jsonResponse({ status: "ok", matches });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/detect/stats") {
        return jsonResponse({ status: "ok", stats: getMatchStats() });
      }

      // ── IP Reputation ──
      if (request.method === "GET" && path === "/api/v1/guardian/ips") {
        return jsonResponse({ status: "ok", ips: listIpRecords(), stats: ipGetStats() });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/ips/blacklisted") {
        return jsonResponse({ status: "ok", blacklisted: getBlacklistedIps() });
      }

      const ipMatch = path.match(/^\/api\/v1\/guardian\/ips\/([^/]+)$/);
      if (request.method === "GET" && ipMatch) {
        const record = getIpRecord(decodeURIComponent(ipMatch[1]!));
        if (!record) return jsonResponse({ error: "IP not found" }, { status: 404 });
        return jsonResponse({ status: "ok", ip: record });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/ips/report") {
        const body = await parseBody(request);
        const ip = typeof body.ip === "string" ? body.ip.trim() : "";
        const reason = typeof body.reason === "string" ? body.reason : "manual-report";
        const points = typeof body.points === "number" ? body.points : 10;
        if (!ip) return jsonResponse({ error: "ip is required" }, { status: 400 });

        const record = ipAddScore(ip, points, reason);
        return jsonResponse({ status: "ok", ip: record });
      }

      const unblacklistMatch = path.match(/^\/api\/v1\/guardian\/ips\/([^/]+)\/unblacklist$/);
      if (request.method === "POST" && unblacklistMatch) {
        const ok = ipUnblacklist(decodeURIComponent(unblacklistMatch[1]!));
        return jsonResponse({ status: ok ? "ok" : "not-found", unblacklisted: ok }, ok ? undefined : { status: 404 });
      }

      // ── Rate Anomaly ──
      if (request.method === "POST" && path === "/api/v1/guardian/rate") {
        const body = await parseBody(request);
        const name = typeof body.name === "string" ? body.name : "";
        const count = typeof body.count === "number" ? body.count : 1;
        if (!name) return jsonResponse({ error: "name is required" }, { status: 400 });

        const anomaly = recordRate(name, count);
        return jsonResponse({ status: "ok", anomaly: anomaly || null });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/rate/anomalies") {
        return jsonResponse({ status: "ok", anomalies: getRateAnomalies() });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/rate/trackers") {
        return jsonResponse({ status: "ok", trackers: listRateTrackers() });
      }

      // ── Threat Patterns Management ──
      if (request.method === "GET" && path === "/api/v1/guardian/threat-patterns") {
        return jsonResponse({ status: "ok", patterns: listThreatPatterns() });
      }

      if (request.method === "POST" && path === "/api/v1/guardian/threat-patterns") {
        const body = await parseBody(request);
        const name = typeof body.name === "string" ? body.name.trim() : "";
        if (!name) return jsonResponse({ error: "name is required" }, { status: 400 });

        const pattern = upsertThreatPattern({
          id: typeof body.id === "string" ? body.id : undefined,
          name,
          category: typeof body.category === "string" ? body.category as any : "injection",
          severity: typeof body.severity === "string" ? body.severity as any : "medium",
          patterns: Array.isArray(body.patterns) ? body.patterns.map((p: string) => new RegExp(p, typeof body.flags === "string" ? body.flags : "i")) : [],
          description: typeof body.description === "string" ? body.description : "",
          autoResponse: typeof body.autoResponse === "string" ? body.autoResponse as any : "alert",
        });
        return jsonResponse({ status: "ok", pattern }, { status: 201 });
      }

      const tpMatch = path.match(/^\/api\/v1\/guardian\/threat-patterns\/([^/]+)$/);
      if (request.method === "DELETE" && tpMatch) {
        const deleted = deleteThreatPattern(decodeURIComponent(tpMatch[1]!));
        return jsonResponse({ deleted }, deleted ? undefined : { status: 404 });
      }

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-guardian] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusGuardianCloudRegistrationHeartbeat(baseUrl);

  return {
    server,
    stop: () => {
      stopHeartbeat();
      stopAllMonitoring();
      clearInterval(persistInterval);
      clearInterval(ipDecayTimer);
      persistRulesToState(listRules());
      persistRlToState(listRateLimitConfigs());
      persistAcToState(listAlertConfigs());
      persistAhToState(listAlerts(200));
      persistHealthSnapshots(listHealthSnapshots());
      server.stop();
    },
  };
}


