import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen } from "@testing-library/react";

vi.mock("./components/Canvas/Canvas", () => ({ default: () => <div data-testid="canvas" /> }));
// connectCollab returns Promise<CollabBinding>, not a bare binding — the
// brief's stub returned a plain object, which crashed App's `.then()` call.
vi.mock("./collab/collab", () => ({
  connectCollab: () => Promise.resolve({ setElements() {}, destroy() {} }),
}));

import App from "./App";

// TopBar is deliberately NOT mocked here. Task 9 hid TopBar wholesale when
// embedded, which silently dropped export (downloadPNG/downloadSVG, TopBar's
// only callers) and keyboard help (HelpOverlay, only mounted here) for every
// embedded user — and a TopBar mock is exactly what let that gap pass review
// undetected. The fix renders a compact TopBar when embedded: branding/title
// chrome goes, document actions (export, help) stay. Google Docs shape: the
// product's bar on top, the document's own toolbar beneath it.
describe("App embed mode", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("standalone: renders both the branding/title and the export controls", () => {
    window.history.pushState({}, "", "/");
    render(<App />);
    expect(screen.queryByText("Nexus-Draw")).not.toBeNull();
    expect(screen.getByRole("button", { name: /PNG/i })).not.toBeNull();
    expect(screen.getByRole("button", { name: /SVG/i })).not.toBeNull();
  });

  it("embedded: the export controls are present, not lost with the chrome", () => {
    window.history.pushState({}, "", "/?embed=1");
    render(<App />);
    expect(screen.getByRole("button", { name: /PNG/i })).not.toBeNull();
    expect(screen.getByRole("button", { name: /SVG/i })).not.toBeNull();
  });

  it("embedded: the branding/title is absent — the shell supplies that chrome", () => {
    window.history.pushState({}, "", "/?embed=1");
    render(<App />);
    expect(screen.queryByText("Nexus-Draw")).toBeNull();
  });

  it("still renders the canvas when embedded, so content is not lost", () => {
    window.history.pushState({}, "", "/?embed=1");
    render(<App />);
    expect(screen.queryByTestId("canvas")).not.toBeNull();
  });
});
