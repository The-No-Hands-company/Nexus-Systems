import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import AppFrame, { embedUrl } from "./AppFrame";

const apps = [
  { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", path: "/draw", health: "healthy" as const },
];

describe("embedUrl", () => {
  it("adds the embed flag", () => {
    expect(embedUrl("https://draw.tnhc.dev")).toBe("https://draw.tnhc.dev/?embed=1");
  });

  it("keeps a query the app already had", () => {
    expect(embedUrl("https://draw.tnhc.dev/?board=7")).toBe("https://draw.tnhc.dev/?board=7&embed=1");
  });

  it("does not add it twice", () => {
    expect(embedUrl("https://draw.tnhc.dev/?embed=1")).toBe("https://draw.tnhc.dev/?embed=1");
  });
});

describe("AppFrame", () => {
  it("frames the app with the embed flag", () => {
    render(<AppFrame apps={apps} appId="nexus-draw" />);
    const frame = screen.getByTitle("Draw") as HTMLIFrameElement;
    expect(frame.src).toBe("https://draw.tnhc.dev/?embed=1");
  });

  it("titles the frame so it is reachable by assistive tech", () => {
    render(<AppFrame apps={apps} appId="nexus-draw" />);
    expect(screen.getByTitle("Draw")).toBeTruthy();
  });

  it("says so plainly when the app is unknown, instead of rendering nothing", () => {
    // A blank content area is indistinguishable from a broken shell.
    render(<AppFrame apps={apps} appId="does-not-exist" />);
    expect(screen.getByText(/not found/i)).toBeTruthy();
    expect(screen.queryByTitle("Draw")).toBeNull();
  });
});
