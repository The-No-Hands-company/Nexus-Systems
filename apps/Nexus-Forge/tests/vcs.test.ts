import { describe, expect, it } from "bun:test";
import { VCSFactory } from "../src/backend/vcs/vcs-interface";

describe("VCS Backend", () => {
  it("should return supported backend instances", () => {
    expect(VCSFactory.getBackend("git")).toBeTruthy();
    expect(VCSFactory.getBackend("svn")).toBeTruthy();
    expect(VCSFactory.getBackend("hg")).toBeTruthy();
    expect(VCSFactory.getBackend("pijul")).toBeTruthy();
  });
});
