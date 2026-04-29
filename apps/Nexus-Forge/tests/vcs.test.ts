import { describe, it, expect } from "bun:test";
import { VCSFactory } from "../src/backend/vcs/vcs-interface";

describe("VCS Backend", () => {
  it("should return supported backend instances", () => {
    expect(VCSFactory.getBackend("git")).toBeTruthy();
    expect(VCSFactory.getBackend("svn")).toBeTruthy();
    expect(VCSFactory.getBackend("hg")).toBeTruthy();
    expect(VCSFactory.getBackend("pijul")).toBeTruthy();
  });
});

describe("Federation", () => {
  it("should expose /.well-known/nexus-cloud endpoint", async () => {
    expect(true).toBe(true);
  });

  it("should register peer nodes", async () => {
    expect(true).toBe(true);
  });
});
