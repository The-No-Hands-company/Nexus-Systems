import { describe, expect, it } from "bun:test";
import { assetUrl, resolveCalendarRuntime } from "./runtime";

describe("resolveCalendarRuntime", () => {
  it("resolves the standalone root", () => {
    expect(resolveCalendarRuntime("/")).toEqual({
      basePath: "/",
      apiBase: "/api/v1/calendar",
      publicBase: "/",
      shellContext: false,
    });
  });

  it("resolves a standalone public share token without enabling shell APIs", () => {
    expect(resolveCalendarRuntime("/share/abc123")).toEqual({
      basePath: "/",
      apiBase: "/api/v1/calendar",
      publicBase: "/",
      shellContext: false,
      publicToken: "abc123",
    });
  });

  it("resolves the Dashboard shell at its root and nested routes", () => {
    expect(resolveCalendarRuntime("/calendar")).toMatchObject({
      basePath: "/calendar",
      apiBase: "/ipa/calendar",
      publicBase: "/calendar/",
      shellContext: true,
    });
    expect(resolveCalendarRuntime("/calendar/month")).toMatchObject({
      basePath: "/calendar",
      apiBase: "/ipa/calendar",
      publicBase: "/calendar/",
      shellContext: true,
    });
  });

  it("lets the server-provided marker and bases override path guessing", () => {
    expect(
      resolveCalendarRuntime("/", {
        basePath: "/calendar",
        apiBase: "/ipa/calendar",
        publicBase: "/calendar/",
        shellContext: true,
      }),
    ).toEqual({
      basePath: "/calendar",
      apiBase: "/ipa/calendar",
      publicBase: "/calendar/",
      shellContext: true,
    });
  });

  it("normalizes asset paths against the selected public base", () => {
    const standalone = resolveCalendarRuntime("/");
    const shell = resolveCalendarRuntime("/calendar/month");

    expect(assetUrl("assets/app.js", standalone)).toBe("/assets/app.js");
    expect(assetUrl("assets/app.js", shell)).toBe("/calendar/assets/app.js");
  });
});
