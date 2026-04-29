import { buildSystemsApiRegistrationPayload } from "./contracts";
import { startNexusAuthCloudRegistrationHeartbeat } from "./cloud";
import { listSeedIdentities, seedDefaultIdentities } from "./state";
import { issueServiceToken, validateServiceToken } from "./token";

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

export function createAuthServer() {
  const port = Number(process.env.PORT || "4310");
  const baseUrl = process.env.NEXUS_AUTH_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled = (process.env.NEXUS_AUTH_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  const server = Bun.serve({
    port,
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({
          status: "ok",
          service: "nexus-auth",
          timestamp: new Date().toISOString(),
        });
      }

      if (request.method === "GET" && path === "/api/v1/auth/readiness") {
        const identities = listSeedIdentities();
        return jsonResponse({
          status: "ready",
          contracts: {
            seed: "/api/v1/auth/seed",
            tokenIssue: "/api/v1/auth/token/issue",
            tokenValidate: "/api/v1/auth/token/validate",
          },
          seedIdentityCount: identities.length,
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        });
      }

      if (request.method === "POST" && path === "/api/v1/auth/seed") {
        const body = await parseBody(request);
        const result = seedDefaultIdentities({
          adminUsername: typeof body.adminUsername === "string" ? body.adminUsername : undefined,
          userUsername: typeof body.userUsername === "string" ? body.userUsername : undefined,
        });

        return jsonResponse({
          status: "ok",
          createdCount: result.createdCount,
          identities: result.identities,
        });
      }

      if (request.method === "POST" && path === "/api/v1/auth/token/issue") {
        const body = await parseBody(request);
        const serviceId = typeof body.serviceId === "string" ? body.serviceId.trim() : "";
        if (!serviceId) {
          return jsonResponse({ error: "serviceId is required" }, { status: 400 });
        }

        const issue = issueServiceToken({
          serviceId,
          audience: typeof body.audience === "string" ? body.audience : undefined,
          scopes: Array.isArray(body.scopes)
            ? body.scopes.filter((item): item is string => typeof item === "string" && item.trim().length > 0)
            : undefined,
          expiresInSeconds: typeof body.expiresInSeconds === "number" ? body.expiresInSeconds : undefined,
        });

        return jsonResponse({
          status: "ok",
          token: issue.token,
          payload: issue.payload,
        });
      }

      if (request.method === "POST" && path === "/api/v1/auth/token/validate") {
        const body = await parseBody(request);
        const token = typeof body.token === "string" ? body.token.trim() : "";
        if (!token) {
          return jsonResponse({ error: "token is required" }, { status: 400 });
        }

        const result = validateServiceToken(
          token,
          typeof body.expectedAudience === "string" ? body.expectedAudience : undefined,
        );

        if (!result.valid) {
          return jsonResponse({ valid: false, reason: result.reason }, { status: 401 });
        }

        return jsonResponse(result);
      }

      if (request.method === "GET" && path === "/api/v1/auth/contracts/systems-api-registration") {
        return jsonResponse({
          status: "ok",
          payload: buildSystemsApiRegistrationPayload(baseUrl),
        });
      }

      return jsonResponse({ error: "not found" }, { status: 404 });
    },
  });

  const stopCloudHeartbeat = startNexusAuthCloudRegistrationHeartbeat(baseUrl);
  const stopSignals: NodeJS.Signals[] = ["SIGINT", "SIGTERM"];
  for (const signal of stopSignals) {
    process.once(signal, () => {
      stopCloudHeartbeat();
      server.stop(true);
    });
  }

  return { server, port, baseUrl };
}
