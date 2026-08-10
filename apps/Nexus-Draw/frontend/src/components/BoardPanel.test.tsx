import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import BoardPanel from "./BoardPanel";
import * as api from "../utils/api";

const boards = [
  { id: "b1", name: "Alpha", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean" as const, gridSnap: false, elements: [], collaborators: [], createdAt: "x", updatedAt: "x" },
  { id: "b2", name: "Beta", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "sketch" as const, gridSnap: true, elements: [], collaborators: [], createdAt: "y", updatedAt: "y" },
];

describe("BoardPanel", () => {
  beforeEach(() => {
    vi.spyOn(api, "listBoards").mockResolvedValue(boards as never);
    vi.spyOn(api, "createBoard").mockResolvedValue(boards[0] as never);
    vi.spyOn(api, "deleteBoard").mockResolvedValue(undefined as never);
  });

  it("renders server boards and switches on click", async () => {
    const onSwitch = vi.fn();
    render(<BoardPanel onSwitch={onSwitch} onNew={vi.fn()} />);
    await waitFor(() => expect(screen.getByText("Alpha")).toBeDefined());
    fireEvent.click(screen.getAllByText("Open")[0]!);
    expect(onSwitch).toHaveBeenCalledWith("b1");
  });

  it("creates a new board from the input", async () => {
    const onNew = vi.fn();
    render(<BoardPanel onSwitch={vi.fn()} onNew={onNew} />);
    await waitFor(() => expect(screen.getByPlaceholderText("New board name")).toBeDefined());
    fireEvent.change(screen.getByPlaceholderText("New board name"), { target: { value: "Gamma" } });
    fireEvent.click(screen.getByText("Create"));
    await waitFor(() => expect(onNew).toHaveBeenCalledWith("Gamma"));
  });

  it("deletes a board after confirm", async () => {
    vi.spyOn(window, "confirm").mockReturnValue(true);
    render(<BoardPanel onSwitch={vi.fn()} onNew={vi.fn()} />);
    await waitFor(() => expect(screen.getAllByText("Delete")[0]).toBeDefined());
    fireEvent.click(screen.getAllByText("Delete")[0]!);
    await waitFor(() => expect(api.deleteBoard).toHaveBeenCalledWith("b1"));
  });
});