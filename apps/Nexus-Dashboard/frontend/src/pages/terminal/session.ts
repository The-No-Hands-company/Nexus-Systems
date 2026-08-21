import { FitAddon } from "@xterm/addon-fit";
import { Terminal, type ITerminalAddon } from "@xterm/xterm";

export type SessionState =
  | "connecting"
  | "connected"
  | "disconnected"
  | "refused"
  | "disabled"
  | "unavailable"
  | "limit";

export type TerminalSessionController = {
  id: string;
  startedAt: number;
  mount(element: HTMLElement): void;
  focus(): void;
  fit(): void;
  dispose(): void;
};

type Disposable = { dispose(): void };

type TerminalLike = {
  cols: number;
  rows: number;
  open(element: HTMLElement): void;
  loadAddon(addon: ITerminalAddon): void;
  onData(listener: (data: string) => void): Disposable;
  write(data: string | Uint8Array): void;
  focus(): void;
  dispose(): void;
};

type FitAddonLike = ITerminalAddon & { fit(): void };

type SocketLike = {
  readonly readyState: number;
  binaryType?: BinaryType;
  send(data: string): void;
  close(): void;
  addEventListener(type: string, listener: EventListener): void;
  removeEventListener(type: string, listener: EventListener): void;
};

export type CreateTerminalSessionOptions = {
  id: string;
  now?: () => number;
  onStateChange?: (state: SessionState) => void;
  createTerminal?: () => TerminalLike;
  createFitAddon?: (terminal: TerminalLike) => FitAddonLike;
  createSocket?: (url: string) => SocketLike;
};

/**
 * Maps the Dashboard relay's close outcomes to the message a terminal tab can
 * act on. 1013 is Nexus-Terminal's documented session ceiling; 1011 is the
 * relay's normalized upstream failure. 4003/4004 are reserved for a relay
 * that can report a post-upgrade refusal/disabled state directly.
 */
function stateForCloseCode(code: number): SessionState {
  if (code === 1000 || code === 1001) return "disconnected";
  if (code === 1008 || code === 4003) return "refused";
  if (code === 4004) return "disabled";
  if (code === 1013) return "limit";
  return "unavailable";
}

export function createTerminalSession(opts: CreateTerminalSessionOptions): TerminalSessionController {
  const terminal = opts.createTerminal?.() ?? new Terminal();
  const fitAddon = opts.createFitAddon?.(terminal) ?? new FitAddon();
  const startedAt = (opts.now ?? Date.now)();
  let socket: SocketLike | undefined;
  let mounted = false;
  let disposed = false;
  let state: SessionState | undefined;

  function report(next: SessionState): void {
    if (disposed || state === next) return;
    state = next;
    opts.onStateChange?.(next);
  }

  function onOpen(): void {
    report("connected");
  }

  function onMessage(event: Event): void {
    if (disposed) return;
    const data = (event as MessageEvent<unknown>).data;
    if (typeof data === "string") terminal.write(data);
    else if (data instanceof ArrayBuffer) terminal.write(new Uint8Array(data));
  }

  function onClose(event: Event): void {
    if (disposed) return;
    report(stateForCloseCode((event as CloseEvent).code));
  }

  function onError(): void {
    report("unavailable");
  }

  terminal.loadAddon(fitAddon);
  const inputSubscription = terminal.onData((data) => {
    if (!disposed && socket?.readyState === WebSocket.OPEN) socket.send(data);
  });

  return {
    id: opts.id,
    startedAt,
    mount(element) {
      if (mounted || disposed) return;
      mounted = true;
      terminal.open(element);
      fitAddon.fit();

      const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
      const url = `${protocol}//${window.location.host}/api/terminal/attach?cols=${terminal.cols}&rows=${terminal.rows}`;
      socket = (opts.createSocket ?? ((address) => new WebSocket(address)))(url);
      socket.binaryType = "arraybuffer";
      socket.addEventListener("open", onOpen);
      socket.addEventListener("message", onMessage);
      socket.addEventListener("close", onClose);
      socket.addEventListener("error", onError);
      report("connecting");
    },
    focus() {
      if (!disposed) terminal.focus();
    },
    fit() {
      if (!disposed) fitAddon.fit();
    },
    dispose() {
      if (disposed) return;
      disposed = true;
      inputSubscription.dispose();
      if (socket) {
        socket.removeEventListener("open", onOpen);
        socket.removeEventListener("message", onMessage);
        socket.removeEventListener("close", onClose);
        socket.removeEventListener("error", onError);
        socket.close();
      }
      terminal.dispose();
    },
  };
}
