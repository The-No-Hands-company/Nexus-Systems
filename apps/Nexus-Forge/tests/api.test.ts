import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type TestForge, startForge } from "./helpers/forge";

let forge: TestForge;
let token: string;
beforeAll(() => {
  forge = startForge();
  token = forge.user("someone").token;
});
afterAll(() => forge.stop());

describe("placeholder routes are quarantined", () => {
  // These answered every request with canned success. A forge whose
  // signature "verify" says yes to anything is worse than one with none.
  it("no longer serves the placeholder endpoints, signed in or not", async () => {
    for (const [method, path] of [
      ["POST", "/api/commit-signing/verify"],
      ["GET", "/api/commit-signing/policy"],
      ["GET", "/api/permissions/check?repo=x"],
      ["POST", "/api/cloud/register"],
      ["GET", "/api/cloud/discovery"],
      ["GET", "/api/branch-protection/violations"],
      ["GET", "/api/billing/plans"],
      ["POST", "/api/teams"],
    ] as const) {
      for (const auth of [undefined, `Bearer ${token}`]) {
        const response = await forge.fetch(
          new Request(`${forge.url}${path}`, {
            method,
            headers: auth ? { authorization: auth } : {},
            ...(method === "POST" ? { body: "{}" } : {}),
          }),
        );
        expect({ path, status: response.status }).toEqual({ path, status: 404 });
      }
    }
  });

  it("still answers health and the discovery manifest", async () => {
    expect((await forge.fetch(new Request(`${forge.url}/health`))).status).toBe(200);
    const manifest = await forge.fetch(new Request(`${forge.url}/.well-known/nexus-cloud`));
    expect(((await manifest.json()) as { service: string }).service).toBe("nexus-forge");
  });
});
