import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { EmptyState } from "../../../../../../packages/nexus-design/src/components/ui/empty-state";
import { Skeleton } from "../../../../../../packages/nexus-design/src/components/ui/skeleton";

describe("EmptyState", () => {
  it("shows the title and the hint", () => {
    render(<EmptyState title="Nothing today" hint="Events you create will appear here" />);
    expect(screen.getByText("Nothing today")).toBeTruthy();
    expect(screen.getByText("Events you create will appear here")).toBeTruthy();
  });
});

describe("Skeleton", () => {
  it("announces itself as busy so loading is not read as empty", () => {
    render(<Skeleton />);
    expect(screen.getByRole("status").getAttribute("aria-busy")).toBe("true");
  });

  it("renders the requested number of lines", () => {
    const { container } = render(<Skeleton lines={5} />);
    expect(container.querySelectorAll("[data-skeleton-line]")).toHaveLength(5);
  });
});
