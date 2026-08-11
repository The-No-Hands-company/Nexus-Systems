import { beforeEach, describe, expect, it, vi } from "vitest";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import AIPanel from "./AIPanel";
import * as api from "../utils/api";

const sb = {
  id: "b1", name: "N", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false,
  defaultStyleMode: "clean" as const, gridSnap: false,
  elements: [{ id: "g1", elementType: "rectangle", data: { x: 0, y: 0 }, style: {}, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }],
  collaborators: [], createdAt: "x", updatedAt: "x",
};

describe("AIPanel", () => {
  beforeEach(() => {
    vi.spyOn(api, "generateDiagram").mockResolvedValue({ elements: [sb.elements[0] as never], board_id: "b1" });
    vi.spyOn(api, "getBoard").mockResolvedValue(sb as never);
  });

  it("generates and reports N elements", async () => {
    render(<AIPanel boardId="b1" currentElements={sb.elements as never} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "login flow" } });
    fireEvent.click(screen.getByText("Generate"));
    await waitFor(() => expect(screen.getByText(/added 1 element/i)).toBeTruthy());
    expect(api.generateDiagram).toHaveBeenCalledWith("login flow", "b1");
  });

  it("shows an error when generation fails", async () => {
    vi.spyOn(api, "generateDiagram").mockRejectedValue(new Error("boom"));
    render(<AIPanel boardId="b1" currentElements={[]} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "x" } });
    fireEvent.click(screen.getByText("Generate"));
    await waitFor(() => expect(screen.getByText(/error/i)).toBeTruthy());
  });

  it("disables the button while generating", async () => {
    render(<AIPanel boardId="b1" currentElements={[]} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "x" } });
    fireEvent.click(screen.getByText("Generate"));
    expect((screen.getByText("Generating…") as HTMLButtonElement).disabled).toBe(true);
  });
});
