import { describe, it, expect } from "bun:test";
import { toAppEntries } from "../src/apps";

const AUTH = "auth.tnhc.dev";

const TOOLS = {
  tools: [
    { id: "nexus-cloud", name: "Nexus Cloud", description: "Control panel",
      publicUrl: "https://cloud.tnhc.dev", health: "healthy", registrationStatus: "active" },
    { id: "nexus-auth", name: "Nexus Auth", description: "Identity",
      publicUrl: "https://auth.tnhc.dev", health: "healthy", registrationStatus: "active" },
    { id: "nexus-chat", name: "Nexus Chat", description: "Chat",
      publicUrl: "https://chat.tnhc.dev", health: "healthy", registrationStatus: "registered" },
    { id: "nexus-video", name: "Nexus Video", description: "Video",
      health: "offline", registrationStatus: "offline" },
  ],
};

describe("app grid entries", () => {
  it("keeps only tools a user can actually open", () => {
    const entries = toAppEntries(TOOLS, AUTH);
    expect(entries.map((e) => e.id).sort()).toEqual(["nexus-chat", "nexus-cloud"]);
  });

  it("drops the auth host — it is the sign-in provider, not a destination", () => {
    expect(toAppEntries(TOOLS, AUTH).some((e) => e.url.includes(AUTH))).toBe(false);
  });

  it("drops the scaffolds that have no public URL", () => {
    expect(toAppEntries(TOOLS, AUTH).some((e) => e.id === "nexus-video")).toBe(false);
  });

  it("carries health through so a down app shows as down rather than a dead link", () => {
    const entries = toAppEntries({
      tools: [{ id: "x", name: "X", publicUrl: "https://x.tnhc.dev", health: "offline" }],
    }, AUTH);
    expect(entries[0]!.health).toBe("offline");
  });

  it("sorts by name so the grid does not reshuffle between polls", () => {
    const entries = toAppEntries({
      tools: [
        { id: "b", name: "Zeta", publicUrl: "https://z.tnhc.dev", health: "healthy" },
        { id: "a", name: "Alpha", publicUrl: "https://a.tnhc.dev", health: "healthy" },
      ],
    }, AUTH);
    expect(entries.map((e) => e.name)).toEqual(["Alpha", "Zeta"]);
  });

  it("falls back to the id when a tool has no name", () => {
    const entries = toAppEntries({
      tools: [{ id: "nexus-thing", publicUrl: "https://t.tnhc.dev", health: "healthy" }],
    }, AUTH);
    expect(entries[0]!.name).toBe("nexus-thing");
  });

  it("survives a malformed payload rather than taking the dashboard down", () => {
    expect(toAppEntries(null, AUTH)).toEqual([]);
    expect(toAppEntries({}, AUTH)).toEqual([]);
    expect(toAppEntries({ tools: "nope" }, AUTH)).toEqual([]);
    expect(toAppEntries({ tools: [null, 42] }, AUTH)).toEqual([]);
  });
});

describe("the dashboard does not list itself", () => {
  const payload = {
    tools: [
      { id: "nexus-dashboard", name: "Nexus Dashboard", publicUrl: "https://app.tnhc.dev", health: "healthy" },
      { id: "nexus-draw", name: "Nexus-Draw", publicUrl: "https://draw.tnhc.dev", health: "healthy" },
      { id: "nexus-auth", name: "Nexus Auth", publicUrl: "https://auth.tnhc.dev", health: "healthy" },
    ],
  };

  it("omits its own tile — a button to where you already are", () => {
    const entries = toAppEntries(payload, "auth.tnhc.dev", "app.tnhc.dev");
    expect(entries.map((e) => e.id)).toEqual(["nexus-draw"]);
  });

  it("still omits the identity service", () => {
    const entries = toAppEntries(payload, "auth.tnhc.dev", "app.tnhc.dev");
    expect(entries.some((e) => e.url.includes("auth."))).toBe(false);
  });

  it("without a self host, nothing is dropped but auth", () => {
    const entries = toAppEntries(payload, "auth.tnhc.dev");
    expect(entries.map((e) => e.id)).toEqual(["nexus-dashboard", "nexus-draw"]);
  });
});

describe("Cloud's tile links into the shell, not out to its own host", () => {
  const payload = {
    tools: [
      { id: "nexus-cloud", name: "Nexus Cloud", description: "Control panel",
        publicUrl: "https://cloud.tnhc.dev", health: "healthy" },
      { id: "nexus-draw", name: "Nexus-Draw", publicUrl: "https://draw.tnhc.dev", health: "healthy" },
    ],
  };

  it("rewrites Cloud's url to the shell-native /cloud route", () => {
    const entries = toAppEntries(payload, AUTH, "app.tnhc.dev", "cloud.tnhc.dev");
    expect(entries.find((e) => e.id === "nexus-cloud")?.url).toBe("/cloud");
  });

  it("leaves every other app's url untouched", () => {
    const entries = toAppEntries(payload, AUTH, "app.tnhc.dev", "cloud.tnhc.dev");
    expect(entries.find((e) => e.id === "nexus-draw")?.url).toBe("https://draw.tnhc.dev");
  });

  it("without a cloud host, nothing is rewritten", () => {
    const entries = toAppEntries(payload, AUTH, "app.tnhc.dev");
    expect(entries.find((e) => e.id === "nexus-cloud")?.url).toBe("https://cloud.tnhc.dev");
  });
});

describe("one tile per destination", () => {
  it("collapses two tools that publish the same address", () => {
    // A published site and the backend behind it are two registry records for
    // one thing. The grid showed both, which reads as the app existing twice.
    const entries = toAppEntries(
      {
        tools: [
          { id: "nexus-draw", name: "Nexus-Draw", publicUrl: "https://draw.tnhc.dev", health: "healthy" },
          { id: "nexus-draw-site", name: "Nexus-Draw (site)", publicUrl: "https://draw.tnhc.dev", health: "healthy" },
        ],
      },
      "auth.tnhc.dev",
      "app.tnhc.dev",
    );
    expect(entries).toHaveLength(1);
    expect(entries[0]?.id).toBe("nexus-draw");
  });
});
