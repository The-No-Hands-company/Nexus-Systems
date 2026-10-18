import { describe, expect, it, vi } from "vitest";
import { fireEvent, render, screen } from "@testing-library/react";
import CatalogFilters from "./CatalogFilters";

describe("CatalogFilters", () => {
  it("exposes controlled search and shelf/audience/capability filters", () => {
    const onQueryChange = vi.fn();
    render(<CatalogFilters query="chat" shelf="everyday" audience="all" capability="all" onQueryChange={onQueryChange} onShelfChange={() => {}} onAudienceChange={() => {}} onCapabilityChange={() => {}} capabilities={["chat"]} />);
    expect((screen.getByRole("searchbox") as HTMLInputElement).value).toBe("chat");
    expect((screen.getByRole("combobox", { name: /shelf/i }) as HTMLSelectElement).value).toBe("everyday");
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "draw" } });
    expect(onQueryChange).toHaveBeenCalledWith("draw");
  });
});
