import { describe, expect, it } from "bun:test";
import { buildApiContract } from "../src/api-contract";
import { buildOpenApiDocument } from "../src/openapi";

const NON_OPERATION_KEYS = new Set(["parameters", "summary", "description", "servers", "$ref"]);

function toOpenApiPath(contractPath: string): string {
  return contractPath.replace(/:(\w+)/g, "{$1}");
}

describe("contract / OpenAPI parity", () => {
  const contract = buildApiContract("https://api.tnhc.dev");
  const openapi = buildOpenApiDocument(contract);
  const paths = openapi.paths as Record<string, Record<string, unknown>>;

  it("publishes the public contract base URL on /ipa/v1", () => {
    expect(contract).toMatchObject({
      service: { baseUrl: "https://api.tnhc.dev/ipa/v1" },
    });
  });

  it("every contract route has an OpenAPI operation", () => {
    for (const route of contract.routes) {
      const openApiPath = toOpenApiPath(route.path);
      const method = route.method.toLowerCase();
      const item = paths[openApiPath] as Record<string, unknown> | undefined;
      expect(item).toBeDefined();
      expect(item?.[method]).toBeDefined();
    }
  });

  it("every OpenAPI operation maps to a contract route", () => {
    const contractKeys = new Set(
      contract.routes.map((r) => `${r.method}:${toOpenApiPath(r.path)}`),
    );
    for (const [openApiPath, item] of Object.entries(paths)) {
      for (const key of Object.keys(item as Record<string, unknown>)) {
        if (NON_OPERATION_KEYS.has(key)) continue;
        const routeKey = `${key.toUpperCase()}:${openApiPath}`;
        expect(contractKeys.has(routeKey)).toBe(true);
      }
    }
  });

  it("contract has at least one public, session, and operator route", () => {
    const accesses = contract.routes.map((r) => r.access);
    expect(accesses).toContain("public");
    expect(accesses).toContain("session");
    expect(accesses).toContain("operator");
  });

  it("every route has a non-empty owner and a valid access tier (admission gate)", () => {
    const validAccess = new Set(["public", "session", "operator"]);
    for (const route of contract.routes) {
      expect(route.owner).toBeTruthy();
      expect(validAccess.has(route.access)).toBe(true);
    }
  });
});
