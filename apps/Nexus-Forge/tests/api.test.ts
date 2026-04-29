import { describe, it, expect } from "bun:test";
import { VCSFactory } from "../src/backend/vcs/vcs-interface";
import { registerFederationRoutes } from "../src/backend/api/federation";

describe("API Integration", () => {
  it("should provide VCS backends for each supported type", () => {
    expect(VCSFactory.getBackend("git")).toBeTruthy();
    expect(VCSFactory.getBackend("svn")).toBeTruthy();
    expect(VCSFactory.getBackend("hg")).toBeTruthy();
    expect(VCSFactory.getBackend("pijul")).toBeTruthy();
  });

  it("should register federation routes without throwing", () => {
    const mockApp = { get: () => mockApp, post: () => mockApp } as any;
    expect(() => registerFederationRoutes(mockApp)).not.toThrow();
  });
});
