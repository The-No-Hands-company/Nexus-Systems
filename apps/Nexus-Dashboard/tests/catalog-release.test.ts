import { describe, expect, it } from "bun:test";
import { buildCatalogReleaseFixture } from "../../Nexus-Cloud/src/catalog/release-fixture";
import { isReleasedUserApp, shellNativeEntries, toAppEntries } from "../src/apps";

const releasedHealthyUserApp = (id: string) => ({
  id,
  name: id,
  description: "released user app",
  publicUrl: `https://${id}.tnhc.dev`,
  path: `/${id.replace(/^nexus-/, "")}`,
  delivery: "framed" as const,
  health: "healthy" as const,
  surface: "user-app" as const,
  releaseState: "released" as const,
  maturity: "stable" as const,
  visibility: "default" as const,
  reviewStatus: "approved" as const,
});

describe("catalog release contract", () => {
  it("keeps only released stable healthy user apps", () => {
    const entries = toAppEntries({ apps: [
      releasedHealthyUserApp("nexus-calendar"),
      { ...releasedHealthyUserApp("nexus-cloud"), surface: "admin-console" },
      { ...releasedHealthyUserApp("nexus-auth"), surface: "platform-service" },
      { ...releasedHealthyUserApp("nexus-video"), releaseState: "staged" },
      { ...releasedHealthyUserApp("nexus-draw"), health: "offline" },
      { ...releasedHealthyUserApp("nexus-preview"), maturity: "preview" },
      { ...releasedHealthyUserApp("nexus-pending"), reviewStatus: "pending" },
      { ...releasedHealthyUserApp("nexus-unreviewed"), reviewStatus: undefined },
    ] }, "auth.tnhc.dev");

    expect(entries.map((entry) => entry.id)).toEqual(["nexus-calendar"]);
    expect(entries.every(isReleasedUserApp)).toBe(true);
  });

  it("fails closed for legacy records without release metadata", () => {
    expect(toAppEntries({
      apps: [{ id: "legacy", name: "Legacy", publicUrl: "https://legacy.tnhc.dev", health: "healthy" }],
    }, "auth.tnhc.dev")).toEqual([]);
  });

  it("rejects unknown release metadata and retired apps", () => {
    const entries = toAppEntries({ apps: [
      { ...releasedHealthyUserApp("bad-surface"), surface: "unknown" },
      { ...releasedHealthyUserApp("bad-state"), releaseState: "unknown" },
      { ...releasedHealthyUserApp("retired"), releaseState: "retired" },
    ] }, "auth.tnhc.dev");
    expect(entries).toEqual([]);
  });

  it("rejects missing and unknown visibility instead of making them discoverable", () => {
    const entries = toAppEntries({ apps: [
      { ...releasedHealthyUserApp("missing-visibility"), visibility: undefined },
      { ...releasedHealthyUserApp("unknown-visibility"), visibility: "unknown" },
      { ...releasedHealthyUserApp("explicit-discoverable"), visibility: "discoverable" },
    ] }, "auth.tnhc.dev");
    expect(entries.map((entry) => entry.id)).toEqual(["explicit-discoverable"]);
  });

  it("rejects unsafe URLs and explicit or derived paths", () => {
    const entries = toAppEntries({ apps: [
      { ...releasedHealthyUserApp("bad-url"), publicUrl: "bad-url" },
      { ...releasedHealthyUserApp("empty-url"), publicUrl: "" },
      { ...releasedHealthyUserApp("empty-native-url"), publicUrl: "", delivery: "shell-native" },
      { ...releasedHealthyUserApp("relative-url"), publicUrl: "/mail", delivery: "shell-native" },
      { ...releasedHealthyUserApp("script-url"), publicUrl: "javascript:alert(1)" },
      { ...releasedHealthyUserApp("bad-route"), path: "/bad?route" },
      { ...releasedHealthyUserApp("root-route"), path: "/" },
      { ...releasedHealthyUserApp(".."), path: undefined, publicUrl: "https://dotdot.tnhc.dev" },
      releasedHealthyUserApp("valid-route"),
    ] }, "auth.tnhc.dev");
    expect(entries.map((entry) => entry.id)).toEqual(["valid-route"]);
  });

  it("accepts a healthy released shell-native app with a valid route and no origin", () => {
    const entries = toAppEntries({ apps: [{
      ...releasedHealthyUserApp("nexus-email"), publicUrl: undefined,
      path: "/mail", delivery: "shell-native",
    }] }, "auth.tnhc.dev");
    expect(entries.map((entry) => entry.id)).toEqual(["nexus-email"]);
  });

  it("excludes Cloud and staged apps from the shared release fixture", () => {
    const entries = toAppEntries(buildCatalogReleaseFixture(), "auth.tnhc.dev");
    expect(entries.map((entry) => entry.id)).toEqual(["nexus-email"]);
    expect(entries[0]).toMatchObject({ surface: "user-app", releaseState: "released" });
  });

  it("gives native Mail and Terminal explicit release metadata", () => {
    const entries = shellNativeEntries({ mailHealthy: true, terminalHealthy: true, includeTerminal: true });
    expect(entries.find((entry) => entry.id === "nexus-email")).toMatchObject({
      surface: "user-app", releaseState: "released", maturity: "stable", reviewStatus: "approved",
    });
    expect(entries.find((entry) => entry.id === "nexus-terminal")).toMatchObject({
      surface: "developer-tool", releaseState: "released", maturity: "stable", reviewStatus: "approved",
    });
  });
});
