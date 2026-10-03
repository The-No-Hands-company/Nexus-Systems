import { describe, expect, it } from "bun:test";
import { isValidRepoName, repoDirName } from "../src/backend/git/names";

describe("repository names", () => {
  it("accepts plain lowercase names", () => {
    for (const name of ["forge", "nexus-forge", "a", "repo_2", "0day"]) {
      expect(isValidRepoName(name)).toBe(true);
    }
  });

  it("rejects every shape that could escape the storage root or become a git option", () => {
    const hostile = [
      "",
      "..",
      "../etc",
      "a/b",
      "a\\b",
      ".hidden",
      "-upload-pack=evil",
      "--help",
      "repo.git",
      "Repo",
      "a b",
      "a\0b",
      "a\nb",
      "x".repeat(65),
      "%2e%2e",
    ];
    for (const name of hostile) {
      expect(isValidRepoName(name)).toBe(false);
    }
    expect(isValidRepoName(undefined)).toBe(false);
    expect(isValidRepoName(42)).toBe(false);
  });

  it("refuses to build a directory name from an invalid name", () => {
    expect(repoDirName("forge")).toBe("forge.git");
    expect(() => repoDirName("../x")).toThrow();
  });
});
