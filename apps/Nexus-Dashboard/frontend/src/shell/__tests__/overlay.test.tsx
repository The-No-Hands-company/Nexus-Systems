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

  it("does not move focus when a re-render passes a new onClose identity while still open", () => {
    // A real, focusable trigger — the same setup as "returns focus to the
    // trigger when it closes" — so the bug (cleanup yanking focus back to
    // whatever was focused before open) has something real to yank to.
    // Without this, restoreTo.current is document.body, and body.focus() is
    // a no-op in jsdom, which would mask the bug entirely.
    const trigger = document.createElement("button");
    document.body.appendChild(trigger);
    trigger.focus();

    const { rerender } = render(
      <Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>,
    );
    const insideButton = screen.getByRole("button", { name: "inside" });
    expect(document.activeElement).toBe(insideButton);

    // Spy AFTER the initial manual trigger.focus() above, so it only
    // observes calls made during the rerender below. A buggy effect with
    // `onClose` in its dependency array tears down on identity change —
    // cleanup calls restoreTo.current.focus() (the trigger) — then
    // immediately re-runs setup, which refocuses the inside button. Both
    // calls happen synchronously inside the same act() flush, so by the
    // time rerender() returns, document.activeElement has already settled
    // back to insideButton — a bare before/after activeElement comparison
    // would not catch the mid-flight steal. Spying on the trigger's own
    // focus() call catches it regardless of where things settle.
    const triggerFocusSpy = vi.spyOn(trigger, "focus");

    // Simulate the real usage pattern: onClose={() => setOpen(false)} is a
    // fresh function identity on every parent render. `open` itself doesn't
    // change here — only onClose's identity does.
    rerender(<Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>);

    expect(triggerFocusSpy).not.toHaveBeenCalled();
    expect(document.activeElement).toBe(insideButton);
  });
});
