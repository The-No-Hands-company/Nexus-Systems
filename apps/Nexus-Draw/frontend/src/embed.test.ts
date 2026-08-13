import { describe, it, expect } from "vitest";
import { isEmbedded } from "./embed";

describe("isEmbedded", () => {
  it("is true when the shell asks for it", () => {
    expect(isEmbedded("?embed=1")).toBe(true);
  });

  it("is false normally", () => {
    expect(isEmbedded("")).toBe(false);
  });

  it("ignores other values, so ?embed=0 does not embed", () => {
    expect(isEmbedded("?embed=0")).toBe(false);
  });

  it("survives other parameters alongside it", () => {
    expect(isEmbedded("?board=7&embed=1")).toBe(true);
  });
});
