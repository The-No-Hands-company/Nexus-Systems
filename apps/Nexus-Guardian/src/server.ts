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
} from "./state";
import type { GuardianScope, GuardianDecisionVerdict } from "./types";

const VALID_SCOPES: GuardianScope[] = ["service", "user", "agent", "network", "resource"];
const VALID_VERDICTS: GuardianDecisionVerdict[] = ["approve", "deny", "suspend", "quarantine"];

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
  return (VALID_SCOPES as string[]).includes(value);
}

function isValidVerdict(value: string): value is GuardianDecisionVerdict {
  return (VALID_VERDICTS as string[]).includes(value);
}

export function createGuardianServer() {
  const port = Number(process.env.PORT || "4320");
  const baseUrl = process.env.NEXUS_GUARDIAN_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled =
    (process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  const server = Bun.serve({
    port,
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
          },
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/decisions") {
        return jsonResponse({ status: "ok", decisions: listDecisions() });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/audit") {
        return jsonResponse({ status: "ok", auditLog: listAuditLog() });
      }

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
        const rules = incomingRules.filter((rule): rule is string => typeof rule === "string");

        const policy = setPublicExposureDenyRules(rules);
        return jsonResponse({ status: "ok", policy });
      }

      if (request.method === "GET" && path === "/api/v1/guardian/contracts/registration") {
        return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(baseUrl) });
      }

      // POST /api/v1/guardian/:scope/:subjectId/:verdict
      const decisionMatch = path.match(/^\/api\/v1\/guardian\/([^/]+)\/([^/]+)\/([^/]+)$/);
      if (request.method === "POST" && decisionMatch) {
        const [, rawScope, subjectId, rawVerdict] = decisionMatch;

        if (!isValidScope(rawScope)) {
          return jsonResponse(
            { error: `Invalid scope '${rawScope}'. Valid scopes: ${VALID_SCOPES.join(", ")}` },
            { status: 400 },
          );
        }

        if (!isValidVerdict(rawVerdict)) {
          return jsonResponse(
            { error: `Invalid verdict '${rawVerdict}'. Valid verdicts: ${VALID_VERDICTS.join(", ")}` },
            { status: 400 },
          );
        }

        const body = await parseBody(request);
        const reason = typeof body.reason === "string" ? body.reason : undefined;
        const issuedBy = typeof body.issuedBy === "string" ? body.issuedBy : "guardian-api";

        const decision = recordDecision(rawScope, decodeURIComponent(subjectId), rawVerdict, issuedBy, reason);
        return jsonResponse({ status: "ok", decision }, { status: 201 });
      }

      // GET /api/v1/guardian/:scope/:subjectId
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

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-guardian] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusGuardianCloudRegistrationHeartbeat(baseUrl);

  return { server, stop: () => { stopHeartbeat(); server.stop(); } };
}
