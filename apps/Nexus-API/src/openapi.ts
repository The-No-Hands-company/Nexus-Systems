import type { NexusApiContract } from "./api-contract";

type OpenApiDocument = {
  openapi: "3.1.0";
  info: { title: string; version: string; description: string };
  servers: Array<{ url: string }>;
  security: Array<{ bearerAuth: string[] }>;
  paths: Record<string, Record<string, unknown>>;
  components: {
    securitySchemes: Record<string, unknown>;
    schemas: Record<string, unknown>;
  };
};

const jsonResponse = (description: string, schema: Record<string, unknown>) => ({
  description,
  content: { "application/json": { schema } },
});

export function buildOpenApiDocument(contract: NexusApiContract): OpenApiDocument {
  const publicSecurity: Array<Record<string, unknown>> = [{}];
  const sessionSecurity = [{ bearerAuth: [] }, { cookieAuth: [] }];
  const operatorSecurity = [{ operatorToken: [] }, { bearerAuth: [] }];

  return {
    openapi: "3.1.0",
    info: {
      title: "Nexus Platform API",
      version: contract.service.version,
      description:
        "Stable platform facade for Nexus applications. Auth owns identity, Cloud owns control-plane state, and application services own their data.",
    },
    servers: [{ url: contract.service.baseUrl }],
    security: [{ bearerAuth: [] }],
    paths: {
      "/health": {
        get: {
          operationId: "getHealth",
          security: publicSecurity,
          responses: {
            "200": jsonResponse("Service health", { $ref: "#/components/schemas/Health" }),
          },
        },
      },
      "/api/v1/contract": {
        get: {
          operationId: "getContract",
          security: publicSecurity,
          responses: {
            "200": jsonResponse("Versioned API contract", {
              $ref: "#/components/schemas/Contract",
            }),
          },
        },
      },
      "/api/v1/openapi.json": {
        get: {
          operationId: "getOpenApi",
          security: publicSecurity,
          responses: { "200": { description: "OpenAPI document" } },
        },
      },
      "/api/v1/me": {
        get: {
          operationId: "getCurrentUser",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Authenticated user", {
              $ref: "#/components/schemas/IdentityResponse",
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/apps": {
        get: {
          operationId: "listApps",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Released app catalog", {
              $ref: "#/components/schemas/AppCatalog",
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
            "503": jsonResponse("Catalog unavailable", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/catalog": {
        get: {
          operationId: "getCatalogCompatibilityAlias",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Released app catalog", {
              $ref: "#/components/schemas/AppCatalog",
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/folders": {
        get: {
          operationId: "listMailFolders",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Caller-owned mail folders", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/folders/{id}/messages": {
        get: {
          operationId: "listFolderMessages",
          security: sessionSecurity,
          parameters: [{ name: "id", in: "path", required: true, schema: { type: "string" } }],
          responses: {
            "200": jsonResponse("Caller-owned folder messages", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/messages/{id}": {
        get: {
          operationId: "getMailMessage",
          security: sessionSecurity,
          parameters: [{ name: "id", in: "path", required: true, schema: { type: "string" } }],
          responses: {
            "200": jsonResponse("Caller-owned message", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/messages/{id}/seen": {
        post: {
          operationId: "setMailMessageSeen",
          security: sessionSecurity,
          parameters: [{ name: "id", in: "path", required: true, schema: { type: "string" } }],
          responses: {
            "200": jsonResponse("Message state updated", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/messages": {
        post: {
          operationId: "sendMailMessage",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Message accepted", { type: "object", additionalProperties: true }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/search": {
        get: {
          operationId: "searchMail",
          security: sessionSecurity,
          responses: {
            "200": jsonResponse("Caller-owned search results", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/threads/{id}/messages": {
        get: {
          operationId: "listThreadMessages",
          security: sessionSecurity,
          parameters: [{ name: "id", in: "path", required: true, schema: { type: "string" } }],
          responses: {
            "200": jsonResponse("Caller-owned thread messages", {
              type: "object",
              additionalProperties: true,
            }),
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/mail/messages/{id}/attachments/{index}": {
        get: {
          operationId: "downloadMailAttachment",
          security: sessionSecurity,
          parameters: [
            { name: "id", in: "path", required: true, schema: { type: "string" } },
            { name: "index", in: "path", required: true, schema: { type: "integer", minimum: 0 } },
          ],
          responses: {
            "200": { description: "Caller-owned attachment" },
            "401": jsonResponse("Authentication required", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
      "/api/v1/gateway/status": {
        get: {
          operationId: "getGatewayStatus",
          security: publicSecurity,
          responses: {
            "200": jsonResponse("Gateway counters and uptime", {
              $ref: "#/components/schemas/GatewayStatus",
            }),
          },
        },
      },
      "/api/v1/gateway/routes": {
        get: {
          operationId: "listGatewayRoutes",
          security: operatorSecurity,
          responses: {
            "200": jsonResponse("Configured compatibility routes", {
              $ref: "#/components/schemas/GatewayRoutes",
            }),
            "401": jsonResponse("Operator authentication required", {
              $ref: "#/components/schemas/Error",
            }),
          },
        },
        post: {
          operationId: "upsertGatewayRoute",
          security: operatorSecurity,
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: { $ref: "#/components/schemas/GatewayRouteInput" },
              },
            },
          },
          responses: {
            "200": jsonResponse("Route configured", {
              $ref: "#/components/schemas/GatewayRoute",
            }),
            "400": jsonResponse("Invalid route", { $ref: "#/components/schemas/Error" }),
            "401": jsonResponse("Operator authentication required", {
              $ref: "#/components/schemas/Error",
            }),
          },
        },
      },
      "/api/v1/gateway/routes/{method}/{path}": {
        delete: {
          operationId: "deleteGatewayRoute",
          security: operatorSecurity,
          parameters: [
            { name: "method", in: "path", required: true, schema: { type: "string" } },
            { name: "path", in: "path", required: true, schema: { type: "string" } },
          ],
          responses: {
            "200": jsonResponse("Route removed", {
              type: "object",
              required: ["deleted"],
              properties: { deleted: { type: "boolean" } },
            }),
            "401": jsonResponse("Operator authentication required", {
              $ref: "#/components/schemas/Error",
            }),
            "404": jsonResponse("Route not found", { $ref: "#/components/schemas/Error" }),
          },
        },
      },
    },
    components: {
      securitySchemes: {
        bearerAuth: { type: "http", scheme: "bearer" },
        cookieAuth: { type: "apiKey", in: "cookie", name: "nexus_session" },
        operatorToken: { type: "apiKey", in: "header", name: "x-api-key" },
      },
      schemas: {
        Health: {
          type: "object",
          required: ["service", "status", "version"],
          properties: {
            service: { type: "string", const: "nexus-api" },
            status: { type: "string", enum: ["ok"] },
            version: { type: "string" },
            uptimeSeconds: { type: "integer", minimum: 0 },
          },
        },
        IdentityResponse: {
          type: "object",
          required: ["user"],
          properties: { user: { type: "object", additionalProperties: true } },
        },
        AppCatalog: {
          type: "object",
          required: ["apps"],
          properties: {
            apps: { type: "array", items: { type: "object", additionalProperties: true } },
          },
        },
        GatewayRoute: {
          type: "object",
          required: ["method", "path", "target", "enabled"],
          properties: {
            method: { type: "string" },
            path: { type: "string" },
            target: { type: "string", format: "uri" },
            enabled: { type: "boolean" },
          },
        },
        GatewayRouteInput: {
          type: "object",
          required: ["method", "path", "target"],
          properties: {
            method: { type: "string" },
            path: { type: "string" },
            target: { type: "string", format: "uri" },
            enabled: { type: "boolean", default: true },
          },
        },
        GatewayRoutes: {
          type: "object",
          required: ["routes"],
          properties: {
            routes: { type: "array", items: { $ref: "#/components/schemas/GatewayRoute" } },
          },
        },
        GatewayStatus: {
          type: "object",
          required: ["service", "routesDefined", "routedTotal", "errorsTotal"],
          properties: {
            service: { type: "string", const: "nexus-api" },
            routesDefined: { type: "integer", minimum: 0 },
            routedTotal: { type: "integer", minimum: 0 },
            errorsTotal: { type: "integer", minimum: 0 },
            uptimeSeconds: { type: "integer", minimum: 0 },
          },
        },
        Error: {
          type: "object",
          required: ["error"],
          properties: { error: { type: "string" }, requestId: { type: "string" } },
        },
        Contract: { type: "object", additionalProperties: true },
      },
    },
  };
}
