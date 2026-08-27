import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import HealthStrip from "../HealthStrip";
import type { AppEntry } from "../../api";

function app(id: string, health: AppEntry["health"]): AppEntry {
  return { id, name: id, description: "", url: "", path: `/${id}`, health };
}

describe("HealthStrip", () => {
  it("summarises how many are healthy", () => {
    render(
      <MemoryRouter>
        <HealthStrip apps={[app("a", "healthy"), app("b", "offline")]} />
      </MemoryRouter>,
    );
    expect(screen.getByText("1 of 2 healthy")).toBeTruthy();
  });

  it("shows offline services first, because those are the ones that matter", () => {
    render(
      <MemoryRouter>
        <HealthStrip apps={[app("aaa", "healthy"), app("zzz", "offline")]} />
      </MemoryRouter>,
    );
    const pills = screen.getAllByTestId("health-pill");
    expect(pills[0]!.textContent).toContain("zzz");
  });

  it("caps the strip rather than wrapping 86 pills across the page", () => {
    const many = Array.from({ length: 20 }, (_, i) => app(`svc-${i}`, "healthy"));
    render(
      <MemoryRouter>
        <HealthStrip apps={many} />
      </MemoryRouter>,
    );
    expect(screen.getAllByTestId("health-pill").length).toBeLessThanOrEqual(8);
  });

  it("includes status text so color-blind and screen-reader users can tell healthy from offline", () => {
    render(
      <MemoryRouter>
        <HealthStrip apps={[app("svc-a", "healthy"), app("svc-b", "offline")]} />
      </MemoryRouter>,
    );
    const pills = screen.getAllByTestId("health-pill");
    const healthyPill = pills.find((p) => p.textContent?.includes("svc-a"));
    const offlinePill = pills.find((p) => p.textContent?.includes("svc-b"));
    expect(healthyPill?.textContent).toContain("healthy");
    expect(offlinePill?.textContent).toContain("offline");
  });
});
