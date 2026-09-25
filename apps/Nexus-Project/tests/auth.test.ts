import { afterEach, describe, expect, it } from "bun:test";
import { resolveCaller } from "../src/auth";

const SECRET = "auth-test-hop-secret"; // pragma: allowlist secret

function request(headers: Record<string, string>): Request {
  return new Request("http://project.test/api/v1/project/workspaces", { headers });
}

// Restore every variable a test sets, so nothing leaks into later files.
const ORIGINAL = {
  NEXUS_PROJECT_DASHBOARD_SECRET: process.env.NEXUS_PROJECT_DASHBOARD_SECRET,
  NEXUS_AUTH_INTERNAL_URL: process.env.NEXUS_AUTH_INTERNAL_URL,
};

afterEach(() => {
  for (const [name, value] of Object.entries(ORIGINAL)) {
    if (value === undefined) Reflect.deleteProperty(process.env, name);
    else process.env[name] = value;
  }
});

describe("resolveCaller", () => {
  it("accepts Dashboard's subject when the deployment secret accompanies it", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    const caller = await resolveCaller(
      request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": SECRET }),
    );
    expect(caller).toEqual({ subject: "usr-alice" });
  });

  it("ignores a bare x-nexus-subject, which anyone can type", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    expect(await resolveCaller(request({ "x-nexus-subject": "usr-alice" }))).toBeNull();
  });

  it("refuses a wrong secret, including one of a different length", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    for (const wrong of ["auth-test-hop-secreX", "short", `${SECRET}-longer`]) {
      expect(
        await resolveCaller(
          request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": wrong }),
        ),
      ).toBeNull();
    }
  });

  it("refuses the hop entirely when no secret is configured", async () => {
    expect(
      await resolveCaller(
        request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": "" }),
      ),
    ).toBeNull();
  });

  it("refuses the right secret without a subject", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    expect(await resolveCaller(request({ "x-nexus-dashboard-secret": SECRET }))).toBeNull();
  });

  it("refuses a hop subject that is not a well-formed Nexus subject", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    for (const subject of ["usr alice", "usr-<script>", "x".repeat(201), "usr/../admin"]) {
      expect(
        await resolveCaller(
          request({ "x-nexus-subject": subject, "x-nexus-dashboard-secret": SECRET }),
        ),
      ).toBeNull();
    }
    expect(
      await resolveCaller(
        request({ "x-nexus-subject": "usr-Alice.1:@_x", "x-nexus-dashboard-secret": SECRET }),
      ),
    ).toEqual({ subject: "usr-Alice.1:@_x" });
  });

  it("refuses an identity token that does not verify", async () => {
    process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:9";
    expect(await resolveCaller(request({ "x-nexus-identity": "not.a.token" }))).toBeNull();
  });
});
