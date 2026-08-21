import { act, fireEvent, render, screen, waitFor, within } from "@testing-library/react";
import { StrictMode } from "react";
import { describe, expect, it, vi } from "vitest";
import type { Me } from "../../api";
import type { CreateTerminalSessionOptions, SessionState, TerminalSessionController } from "./session";
import TerminalView from "./TerminalView";

vi.mock("@xterm/xterm", () => ({ Terminal: class {} }));
vi.mock("@xterm/addon-fit", () => ({ FitAddon: class {} }));

const founder: Me = {
  id: "user-founder",
  username: "founder",
  email: "founder@example.test",
  role: "founder",
};

type FakeController = TerminalSessionController & {
  mount: ReturnType<typeof vi.fn>;
  focus: ReturnType<typeof vi.fn>;
  fit: ReturnType<typeof vi.fn>;
  dispose: ReturnType<typeof vi.fn>;
  report(state: SessionState): void;
};

function createHarness({
  focusTerminalOnControllerFocus = false,
}: {
  focusTerminalOnControllerFocus?: boolean;
} = {}) {
  const controllers: FakeController[] = [];
  const createSession = vi.fn((options: CreateTerminalSessionOptions): TerminalSessionController => {
    let terminalFocusTarget: HTMLButtonElement | null = null;
    const controller: FakeController = {
      id: options.id,
      startedAt: 1_725_000_000_000 + controllers.length * 1_000,
      mount: vi.fn((element: HTMLElement) => {
        if (!focusTerminalOnControllerFocus) return;
        terminalFocusTarget = document.createElement("button");
        terminalFocusTarget.type = "button";
        terminalFocusTarget.tabIndex = -1;
        element.append(terminalFocusTarget);
      }),
      focus: vi.fn(() => terminalFocusTarget?.focus()),
      fit: vi.fn(),
      dispose: vi.fn(),
      report(state) {
        options.onStateChange?.(state);
      },
    };
    controllers.push(controller);
    return controller;
  });

  return { controllers, createSession };
}

describe("TerminalView", () => {
  it("keeps exactly one live initial tab through StrictMode effect replay", async () => {
    const { controllers, createSession } = createHarness();

    render(
      <StrictMode>
        <TerminalView user={founder} createSession={createSession} />
      </StrictMode>,
    );

    await waitFor(() => expect(screen.getAllByRole("tab")).toHaveLength(1));
    expect(createSession).toHaveBeenCalledTimes(1);
    expect(controllers).toHaveLength(1);
  });

  it("creates independent tabs and keeps inactive terminal buffers mounted", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} now={() => 1_725_000_002_000} />);

    await waitFor(() => expect(createSession).toHaveBeenCalledTimes(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));

    await waitFor(() => expect(createSession).toHaveBeenCalledTimes(2));
    const panels = screen.getAllByRole("tabpanel", { hidden: true });
    expect(panels).toHaveLength(2);
    expect(panels[0]?.hasAttribute("hidden")).toBe(true);
    expect(panels[1]?.hasAttribute("hidden")).toBe(false);
    expect(controllers[0]?.mount).toHaveBeenCalledTimes(1);
    expect(controllers[1]?.mount).toHaveBeenCalledTimes(1);
  });

  it("switches focus without disposing either live tab", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));
    await waitFor(() => expect(controllers).toHaveLength(2));

    fireEvent.click(screen.getByRole("tab", { name: "Terminal 1" }));

    expect(screen.getByRole("tab", { name: "Terminal 1" }).getAttribute("aria-selected")).toBe("true");
    expect(controllers[0]?.focus).toHaveBeenCalled();
    expect(controllers[0]?.dispose).not.toHaveBeenCalled();
    expect(controllers[1]?.dispose).not.toHaveBeenCalled();
  });

  it("selects and focuses tabs with horizontal keyboard navigation", async () => {
    const { controllers, createSession } = createHarness({
      focusTerminalOnControllerFocus: true,
    });
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));
    await waitFor(() => expect(controllers).toHaveLength(2));
    const second = screen.getByRole("tab", { name: "Terminal 2" });
    second.focus();
    const terminalFocusCalls = controllers[0]?.focus.mock.calls.length;

    fireEvent.keyDown(second, { key: "ArrowLeft" });

    const first = screen.getByRole("tab", { name: "Terminal 1" });
    expect(first.getAttribute("aria-selected")).toBe("true");
    expect(document.activeElement).toBe(first);
    expect(controllers[0]?.focus).toHaveBeenCalledTimes(terminalFocusCalls ?? 0);
  });

  it("closes only the selected tab", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));
    await waitFor(() => expect(controllers).toHaveLength(2));

    const close = screen.getByRole("button", { name: "Close Terminal 1" });
    close.focus();
    fireEvent.click(close);

    expect(controllers[0]?.dispose).toHaveBeenCalledTimes(1);
    expect(controllers[1]?.dispose).not.toHaveBeenCalled();
    expect(screen.queryByRole("tab", { name: "Terminal 1" })).toBeNull();
    const active = screen.getByRole("tab", { name: "Terminal 2" });
    expect(document.activeElement).toBe(active);
  });

  it("moves focus to the replacement tab after closing the active tab", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));
    await waitFor(() => expect(controllers).toHaveLength(2));
    const close = screen.getByRole("button", { name: "Close Terminal 2" });
    close.focus();

    fireEvent.click(close);

    const replacement = screen.getByRole("tab", { name: "Terminal 1" });
    expect(replacement.getAttribute("aria-selected")).toBe("true");
    expect(document.activeElement).toBe(replacement);
  });

  it("offers a new session after the last tab closes", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));

    const close = screen.getByRole("button", { name: "Close Terminal 1" });
    close.focus();
    fireEvent.click(close);

    const newSession = screen.getByRole("button", { name: "New session" });
    expect(document.activeElement).toBe(newSession);
    expect(controllers[0]?.dispose).toHaveBeenCalledTimes(1);
  });

  it("opens session details and disconnects only the active session", async () => {
    const { controllers, createSession } = createHarness();
    render(
      <TerminalView
        user={founder}
        createSession={createSession}
        now={() => 1_725_000_002_000}
      />,
    );
    await waitFor(() => expect(controllers).toHaveLength(1));
    act(() => controllers[0]?.report("connected"));

    fireEvent.click(screen.getByRole("button", { name: "Details" }));

    const details = screen.getByRole("dialog", { name: "Terminal session details" });
    expect(details.getAttribute("aria-modal")).toBeNull();
    expect(details.textContent).toContain("founder");
    expect(details.textContent).toContain("Connected");
    expect(screen.getByText("Audited")).toBeTruthy();
    expect(within(details).getByText("2s")).toBeTruthy();

    fireEvent.click(screen.getByRole("button", { name: "Disconnect" }));

    expect(controllers[0]?.dispose).toHaveBeenCalledTimes(1);
    expect(screen.queryByRole("dialog", { name: "Terminal session details" })).toBeNull();
    expect(screen.getByRole("button", { name: "New session" })).toBeTruthy();
  });

  it("moves focus into details and restores it when Escape closes the dialog", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    const detailsButton = screen.getByRole("button", { name: "Details" });

    fireEvent.click(detailsButton);

    const closeButton = screen.getByRole("button", { name: "Close terminal session details" });
    expect(document.activeElement).toBe(closeButton);

    fireEvent.keyDown(closeButton, { key: "Escape" });

    expect(screen.queryByRole("dialog", { name: "Terminal session details" })).toBeNull();
    expect(document.activeElement).toBe(detailsButton);
  });

  it("closes details on Escape outside the drawer without stealing focus", async () => {
    const { controllers, createSession } = createHarness();
    render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "Details" }));
    const tab = screen.getByRole("tab", { name: "Terminal 1" });
    tab.focus();

    fireEvent.keyDown(tab, { key: "Escape" });

    expect(screen.queryByRole("dialog", { name: "Terminal session details" })).toBeNull();
    expect(document.activeElement).toBe(tab);
  });

  it("disposes every controller exactly once across close and unmount", async () => {
    const { controllers, createSession } = createHarness();
    const view = render(<TerminalView user={founder} createSession={createSession} />);
    await waitFor(() => expect(controllers).toHaveLength(1));
    fireEvent.click(screen.getByRole("button", { name: "New terminal session" }));
    await waitFor(() => expect(controllers).toHaveLength(2));
    fireEvent.click(screen.getByRole("button", { name: "Close Terminal 1" }));

    view.unmount();

    expect(controllers[0]?.dispose).toHaveBeenCalledTimes(1);
    expect(controllers[1]?.dispose).toHaveBeenCalledTimes(1);
  });
});
