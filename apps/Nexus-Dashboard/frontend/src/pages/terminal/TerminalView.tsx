import {
  useCallback,
  useEffect,
  useRef,
  useState,
  type KeyboardEvent,
  type RefObject,
} from "react";
import type { Me } from "../../api";
import {
  createTerminalSession,
  type CreateTerminalSessionOptions,
  type SessionState,
  type TerminalSessionController,
} from "./session";

export type TerminalSessionFactory = (
  options: CreateTerminalSessionOptions,
) => TerminalSessionController;

type TerminalTab = {
  id: string;
  label: string;
  state: SessionState;
  controller: TerminalSessionController;
};

const STATE_LABELS: Record<SessionState, string> = {
  connecting: "Connecting",
  connected: "Connected",
  disconnected: "Disconnected",
  refused: "Access refused",
  disabled: "Terminal disabled",
  unavailable: "Service unavailable",
  limit: "Session limit reached",
};

function elapsedLabel(startedAt: number, now: number): string {
  const seconds = Math.max(0, Math.floor((now - startedAt) / 1_000));
  if (seconds < 60) return `${seconds}s`;
  const minutes = Math.floor(seconds / 60);
  const remainder = seconds % 60;
  if (minutes < 60) return `${minutes}m ${remainder}s`;
  const hours = Math.floor(minutes / 60);
  return `${hours}h ${minutes % 60}m`;
}

function TerminalPanel({
  active,
  controller,
  label,
  preserveTabFocus,
}: {
  active: boolean;
  controller: TerminalSessionController;
  label: string;
  preserveTabFocus: boolean;
}) {
  const mountRef: RefObject<HTMLDivElement> = useRef(null);

  useEffect(() => {
    if (mountRef.current) controller.mount(mountRef.current);
  }, [controller]);

  useEffect(() => {
    if (!active) return;
    controller.fit();
    if (!preserveTabFocus) controller.focus();
  }, [active, controller, preserveTabFocus]);

  return (
    <section
      id={`panel-${controller.id}`}
      role="tabpanel"
      aria-label={label}
      aria-labelledby={`tab-${controller.id}`}
      hidden={!active}
      className="terminal-canvas min-h-0 flex-1 bg-black p-3"
    >
      <div ref={mountRef} className="h-full w-full" />
    </section>
  );
}

export default function TerminalView({
  user,
  createSession = createTerminalSession,
  now = Date.now,
}: {
  user: Me;
  createSession?: TerminalSessionFactory;
  now?: () => number;
}) {
  const controllers = useRef(new Map<string, TerminalSessionController>());
  const nextSessionNumber = useRef(1);
  const [tabs, setTabs] = useState<TerminalTab[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [detailsOpen, setDetailsOpen] = useState(false);
  const [clock, setClock] = useState(() => now());
  const detailsButtonRef = useRef<HTMLButtonElement>(null);
  const detailsCloseRef = useRef<HTMLButtonElement>(null);
  const detailsDrawerRef = useRef<HTMLElement>(null);
  const emptySessionButtonRef = useRef<HTMLButtonElement>(null);
  const restoreDetailsFocus = useRef(false);
  const focusAfterClose = useRef<string | "new-session" | null>(null);
  const keyboardActivatedTab = useRef<string | null>(null);

  const updateState = useCallback((id: string, state: SessionState) => {
    setTabs((current) => current.map((tab) => (tab.id === id ? { ...tab, state } : tab)));
  }, []);

  const newSession = useCallback(() => {
    const number = nextSessionNumber.current++;
    const id = `terminal-${number}`;
    const controller = createSession({
      id,
      onStateChange: (state) => updateState(id, state),
    });
    controllers.current.set(id, controller);
    setTabs((current) => [
      ...current,
      { id, label: `Terminal ${number}`, state: "connecting", controller },
    ]);
    setActiveId(id);
    setDetailsOpen(false);
    return { id, number };
  }, [createSession, updateState]);

  useEffect(() => {
    // Deferring one turn lets StrictMode cancel its first setup during effect
    // replay before any controller (and therefore any PTY) is constructed.
    const timer = window.setTimeout(() => {
      if (controllers.current.size === 0) newSession();
    }, 0);
    return () => window.clearTimeout(timer);
  }, [newSession]);

  useEffect(() => {
    const timer = window.setInterval(() => setClock(now()), 1_000);
    return () => window.clearInterval(timer);
  }, [now]);

  useEffect(() => {
    const onResize = () => {
      if (activeId) controllers.current.get(activeId)?.fit();
    };
    window.addEventListener("resize", onResize);
    return () => window.removeEventListener("resize", onResize);
  }, [activeId]);

  useEffect(() => () => {
    for (const controller of controllers.current.values()) controller.dispose();
    controllers.current.clear();
  }, []);

  useEffect(() => {
    if (detailsOpen) {
      detailsCloseRef.current?.focus();
      return;
    }
    if (restoreDetailsFocus.current) {
      restoreDetailsFocus.current = false;
      detailsButtonRef.current?.focus();
    }
  }, [detailsOpen]);

  useEffect(() => {
    if (!detailsOpen) return;
    const onEscape = (event: globalThis.KeyboardEvent) => {
      if (event.key !== "Escape") return;
      event.preventDefault();
      const focused = document.activeElement;
      restoreDetailsFocus.current = !!focused && !!detailsDrawerRef.current?.contains(focused);
      setDetailsOpen(false);
    };
    window.addEventListener("keydown", onEscape, true);
    return () => window.removeEventListener("keydown", onEscape, true);
  }, [detailsOpen]);

  const closeSession = useCallback((id: string) => {
    const controller = controllers.current.get(id);
    if (controller) {
      controllers.current.delete(id);
      controller.dispose();
    }

    setTabs((current) => {
      const closedIndex = current.findIndex((tab) => tab.id === id);
      const remaining = current.filter((tab) => tab.id !== id);
      setActiveId((selected) => {
        if (selected !== id) {
          focusAfterClose.current = selected
            ?? remaining[closedIndex]?.id
            ?? remaining[closedIndex - 1]?.id
            ?? "new-session";
          return selected;
        }
        const replacement = remaining[closedIndex] ?? remaining[closedIndex - 1] ?? null;
        focusAfterClose.current = replacement?.id ?? "new-session";
        return replacement?.id ?? null;
      });
      return remaining;
    });
    setDetailsOpen(false);
  }, []);

  useEffect(() => {
    const target = focusAfterClose.current;
    if (!target) return;
    const element = target === "new-session"
      ? emptySessionButtonRef.current
      : document.getElementById(`tab-${target}`);
    if (element instanceof HTMLElement) {
      element.focus();
      focusAfterClose.current = null;
    }
  }, [activeId, tabs]);

  const activeTab = tabs.find((tab) => tab.id === activeId) ?? null;

  const selectTabFromKeyboard = (event: KeyboardEvent<HTMLButtonElement>, index: number) => {
    let targetIndex: number | undefined;
    if (event.key === "ArrowLeft") targetIndex = (index - 1 + tabs.length) % tabs.length;
    if (event.key === "ArrowRight") targetIndex = (index + 1) % tabs.length;
    if (event.key === "Home") targetIndex = 0;
    if (event.key === "End") targetIndex = tabs.length - 1;
    if (targetIndex === undefined) return;

    event.preventDefault();
    const target = tabs[targetIndex];
    if (!target) return;
    keyboardActivatedTab.current = target.id;
    setActiveId(target.id);
    document.getElementById(`tab-${target.id}`)?.focus();
  };

  const openDetails = () => {
    restoreDetailsFocus.current = false;
    setDetailsOpen(true);
  };

  const closeDetails = () => {
    restoreDetailsFocus.current = true;
    setDetailsOpen(false);
  };

  return (
    <div className="relative flex h-full min-h-0 flex-col overflow-hidden bg-zinc-900">
      <header className="flex h-11 shrink-0 items-stretch border-b border-zinc-700 bg-zinc-800">
        <div
          role="tablist"
          aria-label="Terminal sessions"
          className="flex min-w-0 flex-1 items-stretch overflow-x-auto"
        >
          {tabs.map((tab, index) => {
            const active = tab.id === activeId;
            return (
              <div
                key={tab.id}
                role="presentation"
                className={`flex shrink-0 items-center border-r border-zinc-700 ${active ? "bg-zinc-900" : "bg-zinc-800"}`}
              >
                <button
                  id={`tab-${tab.id}`}
                  type="button"
                  role="tab"
                  aria-selected={active}
                  aria-controls={`panel-${tab.id}`}
                  tabIndex={active ? 0 : -1}
                  onClick={() => {
                    keyboardActivatedTab.current = null;
                    setActiveId(tab.id);
                  }}
                  onKeyDown={(event) => selectTabFromKeyboard(event, index)}
                  className="h-full px-4 text-sm text-zinc-200 hover:bg-zinc-700"
                >
                  {tab.label}
                </button>
                <button
                  type="button"
                  aria-label={`Close ${tab.label}`}
                  title={`Close ${tab.label}`}
                  onClick={() => closeSession(tab.id)}
                  className="mr-2 rounded px-1.5 py-0.5 text-zinc-500 hover:bg-zinc-700 hover:text-zinc-100"
                >
                  <span aria-hidden="true">×</span>
                </button>
              </div>
            );
          })}
        </div>
        <button
          type="button"
          aria-label="New terminal session"
          title="New terminal session"
          onClick={newSession}
          className="w-11 shrink-0 border-l border-zinc-700 text-lg text-zinc-400 hover:bg-zinc-700 hover:text-zinc-100"
        >
          <span aria-hidden="true">+</span>
        </button>
        <button
          ref={detailsButtonRef}
          type="button"
          onClick={openDetails}
          disabled={!activeTab}
          className="shrink-0 border-l border-zinc-700 px-4 text-sm text-zinc-400 hover:bg-zinc-700 hover:text-zinc-100 disabled:cursor-not-allowed disabled:opacity-40"
        >
          Details
        </button>
      </header>

      {tabs.length === 0 ? (
        <div className="flex min-h-0 flex-1 flex-col items-center justify-center gap-3 p-8 text-center">
          <p className="text-zinc-400">No terminal sessions are open.</p>
          <button
            ref={emptySessionButtonRef}
            type="button"
            onClick={newSession}
            className="rounded-md bg-zinc-100 px-4 py-2 text-sm font-medium text-zinc-900 hover:bg-white"
          >
            New session
          </button>
        </div>
      ) : (
        <div className="relative flex min-h-0 flex-1 flex-col">
          {tabs.map((tab) => (
            <TerminalPanel
              key={tab.id}
              active={tab.id === activeId}
              controller={tab.controller}
              label={tab.label}
              preserveTabFocus={keyboardActivatedTab.current === tab.id}
            />
          ))}
        </div>
      )}

      {activeTab && (
        <footer className="flex h-8 shrink-0 items-center gap-3 border-t border-zinc-700 bg-zinc-800 px-3 text-xs text-zinc-400">
          <span role="status">{STATE_LABELS[activeTab.state]}</span>
          <span aria-hidden="true">•</span>
          <span>Audited</span>
          <span className="ml-auto tabular-nums">
            {elapsedLabel(activeTab.controller.startedAt, clock)}
          </span>
        </footer>
      )}

      {detailsOpen && activeTab && (
        <aside
          ref={detailsDrawerRef}
          role="dialog"
          aria-label="Terminal session details"
          className="absolute inset-y-0 right-0 z-10 flex w-full max-w-sm flex-col border-l border-zinc-700 bg-zinc-800 shadow-2xl"
        >
          <div className="flex h-12 shrink-0 items-center border-b border-zinc-700 px-4">
            <h2 className="font-medium">Session details</h2>
            <button
              ref={detailsCloseRef}
              type="button"
              aria-label="Close terminal session details"
              onClick={closeDetails}
              className="ml-auto rounded px-2 py-1 text-zinc-400 hover:bg-zinc-700 hover:text-zinc-100"
            >
              <span aria-hidden="true">×</span>
            </button>
          </div>
          <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-3 p-4 text-sm">
            <dt className="text-zinc-500">Role</dt>
            <dd>{user.role}</dd>
            <dt className="text-zinc-500">State</dt>
            <dd>{STATE_LABELS[activeTab.state]}</dd>
            <dt className="text-zinc-500">Started</dt>
            <dd>{new Date(activeTab.controller.startedAt).toLocaleString()}</dd>
            <dt className="text-zinc-500">Age</dt>
            <dd>{elapsedLabel(activeTab.controller.startedAt, clock)}</dd>
          </dl>
          <p className="mx-4 rounded-md border border-amber-500/40 bg-amber-500/10 p-3 text-sm text-zinc-300">
            Audited session. Commands and terminal input may be recorded in the operator audit trail.
          </p>
          <button
            type="button"
            onClick={() => closeSession(activeTab.id)}
            className="m-4 mt-auto rounded-md border border-red-500/50 px-4 py-2 text-sm text-red-300 hover:bg-red-500/10"
          >
            Disconnect
          </button>
        </aside>
      )}
    </div>
  );
}
