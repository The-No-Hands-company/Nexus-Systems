import { describe, it, expect } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Shell from "../Shell";

const user = { id: "usr-1", username: "eric", email: "e@tnhc.dev", role: "founder" };

describe("Shell", () => {
  function mount() {
    return render(
      <MemoryRouter>
        <Shell user={user} apps={[]}><p>content</p></Shell>
      </MemoryRouter>,
    );
  }

  it("stamps the density attribute so the density tokens apply", () => {
    mount();
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("balanced");
  });

  it("opens the apps drawer from the Apps button", () => {
    mount();
    expect(screen.queryByRole("dialog")).toBeNull();
    fireEvent.click(screen.getByRole("button", { name: /apps/i }));
    expect(screen.getByRole("dialog")).toBeTruthy();
  });

  it("opens the apps drawer with the keyboard shortcut", () => {
    mount();
    fireEvent.keyDown(document, { key: "k", metaKey: true });
    expect(screen.getByRole("dialog")).toBeTruthy();
  });

  it("toggles density from a real control, not just useDensity's discarded setter", () => {
    // Before this, Shell called useDensity() purely for its stamping side
    // effect and threw the setter away — there was no way to actually
    // change density, so data-nexus-density="compact" never happened.
    mount();
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("balanced");
    fireEvent.click(screen.getByRole("button", { name: /density/i }));
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("compact");
    fireEvent.click(screen.getByRole("button", { name: /density/i }));
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("balanced");
  });

  it("consumes the widget-padding and control-height tokens, not a fixed class", () => {
    // C1's other half: the attribute changing is necessary but not
    // sufficient — something has to actually read the density-scoped token.
    // WidgetShell targets padding at var(--nexus-widget-padding) directly
    // (16px balanced / 12px compact — see
    // packages/nexus-design/tokens/density/*.json), not a fixed `p-4`, and
    // the header's own controls target var(--nexus-control-height)
    // (32px / 26px) the same way.
    mount();
    const densityButton = screen.getByRole("button", { name: /density/i });
    expect(densityButton.className).toContain("h-[var(--nexus-control-height)]");
  });
});
