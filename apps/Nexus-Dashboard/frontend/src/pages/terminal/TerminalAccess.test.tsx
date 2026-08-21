import { render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { Me } from "../../api";
import type { CreateTerminalSessionOptions, TerminalSessionController } from "./session";
import TerminalAccess from "./TerminalAccess";

vi.mock("@xterm/xterm", () => ({ Terminal: class {} }));
vi.mock("@xterm/addon-fit", () => ({ FitAddon: class {} }));

const founder: Me = {
  id: "user-founder",
  username: "founder",
  email: "founder@example.test",
  role: "founder",
};

const admin: Me = {
  id: "user-admin",
  username: "operator",
  email: "operator@example.test",
  role: "admin",
};

const member: Me = {
  id: "user-member",
  username: "member",
  email: "member@example.test",
  role: "member",
};

function controllerFactory() {
  return vi.fn((options: CreateTerminalSessionOptions): TerminalSessionController => ({
    id: options.id,
    startedAt: 1_725_000_000_000,
    mount: vi.fn(),
    focus: vi.fn(),
    fit: vi.fn(),
    dispose: vi.fn(),
  }));
}

describe("TerminalAccess", () => {
  it("does not create a session while the caller identity is still loading", () => {
    const createSession = controllerFactory();

    render(<TerminalAccess userState={{ status: "loading" }} createSession={createSession} />);

    expect(screen.getByText("Checking terminal access…")).toBeTruthy();
    expect(createSession).not.toHaveBeenCalled();
  });

  it("denies an ordinary member without constructing a terminal controller", () => {
    const createSession = controllerFactory();

    render(
      <TerminalAccess
        userState={{ status: "ready", user: member }}
        createSession={createSession}
      />,
    );

    expect(screen.getByText("Terminal access required")).toBeTruthy();
    expect(createSession).not.toHaveBeenCalled();
  });

  it.each([founder, admin])("automatically creates one session for role $role", async (user) => {
    const createSession = controllerFactory();

    render(
      <TerminalAccess
        userState={{ status: "ready", user }}
        createSession={createSession}
      />,
    );

    await waitFor(() => expect(createSession).toHaveBeenCalledTimes(1));
    expect(screen.getByRole("tablist", { name: "Terminal sessions" })).toBeTruthy();
  });
});
