import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen } from "@testing-library/react";

vi.mock("./components/Canvas/Canvas", () => ({ default: () => <div data-testid="canvas" /> }));
vi.mock("./components/TopBar", () => ({ default: () => <div data-testid="topbar" /> }));
// connectCollab returns Promise<CollabBinding>, not a bare binding — the
// brief's stub returned a plain object, which crashed App's `.then()` call.
vi.mock("./collab/collab", () => ({
  connectCollab: () => Promise.resolve({ setElements() {}, destroy() {} }),
}));

import App from "./App";

describe("App embed mode", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("renders its own top bar when standalone", () => {
    window.history.pushState({}, "", "/");
    render(<App />);
    expect(screen.queryByTestId("topbar")).not.toBeNull();
  });

  it("suppresses its own top bar when the shell embeds it", () => {
    window.history.pushState({}, "", "/?embed=1");
    render(<App />);
    expect(screen.queryByTestId("topbar")).toBeNull();
  });

  it("still renders the canvas when embedded, so content is not lost", () => {
    window.history.pushState({}, "", "/?embed=1");
    render(<App />);
    expect(screen.queryByTestId("canvas")).not.toBeNull();
  });
});
