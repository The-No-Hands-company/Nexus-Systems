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
});
