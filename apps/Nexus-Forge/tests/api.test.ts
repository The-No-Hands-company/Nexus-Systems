import { describe, expect, it } from "bun:test";
import { registerFederationRoutes } from "../src/backend/api/federation";
import { VCSFactory } from "../src/backend/vcs/vcs-interface";

describe("API Integration", () => {
  it("should provide VCS backends for each supported type", () => {
    expect(VCSFactory.getBackend("git")).toBeTruthy();
    expect(VCSFactory.getBackend("svn")).toBeTruthy();
    expect(VCSFactory.getBackend("hg")).toBeTruthy();
    expect(VCSFactory.getBackend("pijul")).toBeTruthy();
  });

  it("should register federation routes without throwing", () => {
    const mockApp = { get: () => mockApp, post: () => mockApp } as unknown as ForgeRouteApp;
    expect(() => registerFederationRoutes(mockApp)).not.toThrow();
  });
});
