import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import WidgetShell from "../WidgetShell";

describe("WidgetShell", () => {
  it("shows a busy indicator while loading", () => {
    render(<WidgetShell title="Today" state={{ status: "loading" }} empty="Nothing today">{() => null}</WidgetShell>);
    expect(screen.getByRole("status").getAttribute("aria-busy")).toBe("true");
  });

  it("distinguishes an error from an empty result", () => {
    render(
      <WidgetShell title="Today" state={{ status: "error", message: "Calendar unavailable" }} empty="Nothing today">
        {() => null}
      </WidgetShell>,
    );
    expect(screen.getByText("Calendar unavailable")).toBeTruthy();
    expect(screen.queryByText("Nothing today")).toBeNull();
  });

  it("shows the empty copy only when the request actually succeeded with nothing", () => {
    render(
      <WidgetShell title="Today" state={{ status: "ready", data: [] as number[] }} empty="Nothing today">
        {(rows) => (rows.length ? <p>{rows.length}</p> : null)}
      </WidgetShell>,
    );
    expect(screen.getByText("Nothing today")).toBeTruthy();
  });

  it("renders content when there is some", () => {
    render(
      <WidgetShell title="Today" state={{ status: "ready", data: [1, 2] }} empty="Nothing today">
        {(rows) => <p>{rows.length} events</p>}
      </WidgetShell>,
    );
    expect(screen.getByText("2 events")).toBeTruthy();
  });

  it("pads with the density-scoped widget-padding token, not a fixed p-4", () => {
    // C1: before this, nothing in the shell referenced --nexus-widget-padding
    // (or any density token), so data-nexus-density="compact" changed zero
    // rendered pixels. The widget's own padding is the case the finding
    // names explicitly.
    const { container } = render(
      <WidgetShell title="Today" state={{ status: "ready", data: [] as number[] }} empty="Nothing today">
        {() => null}
      </WidgetShell>,
    );
    const section = container.querySelector("section")!;
    expect(section.className).toContain("p-[var(--nexus-widget-padding)]");
    expect(section.className).not.toContain("p-4");
  });
});
