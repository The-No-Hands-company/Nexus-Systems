import { describe, it, expect, beforeEach } from "vitest";
import { readDensity, applyDensity } from "../useDensity";

beforeEach(() => {
  localStorage.clear();
  document.documentElement.removeAttribute("data-nexus-density");
});

describe("readDensity", () => {
  it("defaults to balanced when nothing is stored", () => {
    expect(readDensity()).toBe("balanced");
  });

  it("reads a stored value", () => {
    localStorage.setItem("nexus.density", "compact");
    expect(readDensity()).toBe("compact");
  });

  it("falls back to balanced for a value it does not recognise", () => {
    // Otherwise a stale or hand-edited key renders the shell with no density
    // tokens at all.
    localStorage.setItem("nexus.density", "enormous");
    expect(readDensity()).toBe("balanced");
  });
});

describe("applyDensity", () => {
  it("stamps the attribute the tokens are scoped to", () => {
    applyDensity("compact");
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("compact");
  });
});

describe("localStorage polyfill fidelity", () => {
  it("round-trips empty strings correctly", () => {
    // This test locks in faithful Storage semantics: setItem coerces to string,
    // getItem returns the exact value back (not null for empty strings).
    // Distinguishes real Storage from broken fakes.
    localStorage.setItem("k", "");
    expect(localStorage.getItem("k")).toBe("");
  });
});
