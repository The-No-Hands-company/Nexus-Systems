import { describe, expect, it, vi } from "vitest";

vi.mock("@xterm/xterm", () => ({ Terminal: class {} }));
vi.mock("@xterm/addon-fit", () => ({ FitAddon: class {} }));

import { createTerminalSession, type SessionState } from "./session";

type Listener = (event: Event) => void;

class FakeTerminal {
  cols = 80;
  rows = 24;
  readonly opened: HTMLElement[] = [];
  readonly writes: string[] = [];
  readonly addons: unknown[] = [];
  readonly onData = vi.fn((listener: (data: string) => void) => {
    this.dataListener = listener;
    return { dispose: this.onDataDispose };
  });
  readonly onDataDispose = vi.fn();
  readonly open = vi.fn((element: HTMLElement) => { this.opened.push(element); });
  readonly loadAddon = vi.fn((addon: unknown) => { this.addons.push(addon); });
  readonly write = vi.fn((data: string) => { this.writes.push(data); });
  readonly focus = vi.fn();
  readonly dispose = vi.fn();
  private dataListener: ((data: string) => void) | undefined;

  emitData(data: string): void {
    this.dataListener?.(data);
  }
}

class FakeFitAddon {
  readonly activate = vi.fn();
  readonly dispose = vi.fn();
  readonly fit = vi.fn(() => {
    this.terminal.cols = 120;
    this.terminal.rows = 40;
  });

  constructor(private readonly terminal: FakeTerminal) {}
}

class FakeSocket {
  static readonly CONNECTING = 0;
  static readonly OPEN = 1;
  readonly listeners = new Map<string, Listener[]>();
  readonly send = vi.fn();
  readonly close = vi.fn();
  readyState = FakeSocket.CONNECTING;

  constructor(readonly url: string) {}

  addEventListener(type: string, listener: Listener): void {
    this.listeners.set(type, [...(this.listeners.get(type) ?? []), listener]);
  }

  removeEventListener(type: string, listener: Listener): void {
    this.listeners.set(type, (this.listeners.get(type) ?? []).filter((item) => item !== listener));
  }

  emit(type: string, event: Event): void {
    for (const listener of this.listeners.get(type) ?? []) listener(event);
  }

  open(): void {
    this.readyState = FakeSocket.OPEN;
    this.emit("open", new Event("open"));
  }

  message(data: string): void {
    this.emit("message", new MessageEvent("message", { data }));
  }

  closed(code: number, reason = ""): void {
    this.emit("close", new CloseEvent("close", { code, reason }));
  }
}

function createHarness() {
  const terminals: FakeTerminal[] = [];
  const sockets: FakeSocket[] = [];
  const states: SessionState[] = [];
  const calls: string[] = [];
  const createTerminal = vi.fn((_options?: { screenReaderMode: boolean }) => {
    calls.push("terminal");
    const terminal = new FakeTerminal();
    terminals.push(terminal);
    return terminal;
  });

  const controller = createTerminalSession({
    id: "session-1",
    now: () => 1_725_000_000_000,
    onStateChange: (state) => states.push(state),
    createTerminal,
    createFitAddon: (terminal) => {
      calls.push("fit-addon");
      return new FakeFitAddon(terminal as FakeTerminal);
    },
    createSocket: (url) => {
      calls.push(`socket:${url}`);
      const socket = new FakeSocket(url);
      sockets.push(socket);
      return socket;
    },
  });

  return { calls, controller, createTerminal, sockets, states, terminals };
}

describe("createTerminalSession", () => {
  it("constructs xterm in screen-reader mode", () => {
    // xterm defaults this option to false, so the semantic tab panel alone
    // does not make terminal output available to assistive technology.
    const { createTerminal } = createHarness();

    expect(createTerminal).toHaveBeenCalledWith({ screenReaderMode: true });
  });

  it("opens and fits before attaching its initial dimensions to the same-origin socket URL", () => {
    // Removing the first renderer fit would reconnect a new PTY at xterm's
    // default 80x24 size rather than the dimensions actually rendered.
    const { calls, controller, sockets, terminals } = createHarness();
    const mount = document.createElement("div");

    controller.mount(mount);

    expect(terminals[0]?.opened).toEqual([mount]);
    expect(calls).toEqual([
      "terminal",
      "fit-addon",
      `socket:ws://${window.location.host}/api/terminal/attach?cols=120&rows=40`,
    ]);
    expect(sockets[0]?.url).toBe(
      `ws://${window.location.host}/api/terminal/attach?cols=120&rows=40`,
    );
  });

  it("sends input only to its own connected socket", () => {
    // Sending while CONNECTING or to another tab's socket crosses session
    // ownership boundaries and can make an operator type into the wrong PTY.
    const first = createHarness();
    const second = createHarness();
    first.controller.mount(document.createElement("div"));
    second.controller.mount(document.createElement("div"));

    first.terminals[0]?.emitData("before-open");
    first.sockets[0]?.open();
    first.terminals[0]?.emitData("first-tab");

    expect(first.sockets[0]?.send).toHaveBeenCalledTimes(1);
    expect(first.sockets[0]?.send).toHaveBeenLastCalledWith("first-tab");
    expect(second.sockets[0]?.send).not.toHaveBeenCalled();
  });

  it("writes socket output through its terminal", () => {
    // Bypassing terminal.write would tempt a view to insert terminal output
    // as DOM and reintroduce an output-injection surface.
    const { controller, sockets, terminals } = createHarness();
    controller.mount(document.createElement("div"));

    sockets[0]?.message("build complete\\r\\n");

    expect(terminals[0]?.write).toHaveBeenCalledTimes(1);
    expect(terminals[0]?.write).toHaveBeenLastCalledWith("build complete\\r\\n");
  });

  it.each([
    [1000, "disconnected"],
    [1008, "refused"],
    [4003, "refused"],
    [4004, "disabled"],
    [1011, "unavailable"],
    [1013, "limit"],
    [1006, "unavailable"],
  ] as const)("maps close code %i to %s", (code, state) => {
    // Mapping a meaningful relay close code to a generic disconnect hides the
    // only actionable reason a failed terminal tab has.
    const { controller, sockets, states } = createHarness();
    controller.mount(document.createElement("div"));

    sockets[0]?.closed(code);

    expect(states).toEqual(["connecting", state]);
  });

  it("never interprets an arbitrary close reason as a trusted failure state", () => {
    const { controller, sockets, states } = createHarness();
    controller.mount(document.createElement("div"));

    sockets[0]?.closed(4999, "terminal disabled");

    expect(states).toEqual(["connecting", "unavailable"]);
  });

  it("refits only the renderer and never emits a resize protocol frame", () => {
    // The backend PTY has initial-size-only support, so a fit must not pretend
    // that the server-side terminal was resized.
    const { controller, sockets, terminals } = createHarness();
    controller.mount(document.createElement("div"));
    sockets[0]?.open();
    const fit = terminals[0]?.addons[0] as FakeFitAddon;
    const sendsBeforeFit = sockets[0]?.send.mock.calls.length;

    controller.fit();

    expect(fit.fit).toHaveBeenCalledTimes(2);
    expect(sockets[0]?.send).toHaveBeenCalledTimes(sendsBeforeFit ?? 0);
  });

  it("disposes its socket, input subscription, and terminal exactly once", () => {
    // A tab can unmount as the socket closes; double cleanup can duplicate the
    // terminal audit end event and leave sibling tabs in an indeterminate state.
    const { controller, sockets, terminals } = createHarness();
    controller.mount(document.createElement("div"));

    controller.dispose();
    controller.dispose();
    sockets[0]?.closed(1000);

    expect(sockets[0]?.close).toHaveBeenCalledTimes(1);
    expect(terminals[0]?.onDataDispose).toHaveBeenCalledTimes(1);
    expect(terminals[0]?.dispose).toHaveBeenCalledTimes(1);
  });
});
