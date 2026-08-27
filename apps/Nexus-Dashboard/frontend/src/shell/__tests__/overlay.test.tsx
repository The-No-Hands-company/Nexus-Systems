import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { Overlay } from "../../../../../../packages/nexus-design/src/components/ui/overlay";

describe("Overlay", () => {
  it("renders nothing when closed", () => {
    render(<Overlay open={false} onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(screen.queryByRole("dialog")).toBeNull();
  });

  it("exposes itself as a labelled modal dialog", () => {
    render(<Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    const dialog = screen.getByRole("dialog");
    expect(dialog.getAttribute("aria-modal")).toBe("true");
    expect(dialog.getAttribute("aria-label")).toBe("Apps");
  });

  it("closes on Escape", () => {
    const onClose = vi.fn();
    render(<Overlay open onClose={onClose} label="Apps"><button>inside</button></Overlay>);
    fireEvent.keyDown(document, { key: "Escape" });
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("moves focus inside when it opens", () => {
    render(<Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(document.activeElement).toBe(screen.getByRole("button", { name: "inside" }));
  });

  it("returns focus to the trigger when it closes", () => {
    const trigger = document.createElement("button");
    document.body.appendChild(trigger);
    trigger.focus();
    const { rerender } = render(
      <Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>,
    );
    rerender(<Overlay open={false} onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(document.activeElement).toBe(trigger);
  });
});
