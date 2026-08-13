import { useCallback, useEffect, useState } from "react";
import { BrowserRouter, Routes, Route, useParams } from "react-router-dom";
import Home from "./pages/Home";
import RequestAccess from "./pages/RequestAccess";
import Claim from "./pages/Claim";
import Account from "./pages/Account";
import Admin from "./pages/Admin";
import { listApps, type AppEntry } from "./api";
import Shell from "./shell/Shell";
import Launcher from "./shell/Launcher";
import AppFrame from "./shell/AppFrame";

/**
 * The app list has three states, not two: while it is loading, "not found"
 * would be a lie about an app that may well exist, and a failed fetch must
 * not be silently reported as the same thing — the user needs a different
 * action (retry) than they would for a genuinely unknown app id.
 */
type AppsState =
  | { status: "loading" }
  | { status: "ready"; apps: AppEntry[] }
  | { status: "failed" };

/** Renders a single ecosystem app inside the shell's chrome, keyed off the URL. */
function ShellRoute({ state, onRetry }: { state: AppsState; onRetry: () => void }) {
  const { appId = "" } = useParams();
  const apps = state.status === "ready" ? state.apps : [];

  return (
    <Shell sidebar={<Launcher apps={apps} activeId={appId} />}>
      {state.status === "loading" && (
        <div className="flex h-full items-center justify-center p-8 text-text-muted">
          Loading…
        </div>
      )}
      {state.status === "failed" && (
        <div className="flex h-full flex-col items-center justify-center gap-3 p-8 text-center">
          <p className="text-text-primary">Could not load your apps.</p>
          <p className="text-sm text-text-muted">
            This is not the same as the app being missing — try again.
          </p>
          <button
            type="button"
            onClick={onRetry}
            className="rounded border border-border-subtle px-4 py-2 text-sm hover:bg-bg-elevated"
          >
            Retry
          </button>
        </div>
      )}
      {state.status === "ready" && <AppFrame apps={state.apps} appId={appId} />}
    </Shell>
  );
}

/**
 * Plain paths, and the dashboard server serves index.html for any unmatched
 * path, so a hard reload on /claim works rather than 404ing.
 *
 * Home decides between the app grid and the signed-out landing; account and
 * admin surfaces land in the next tasks. /request and /claim are public pages
 * for people with no session and no apps yet, so they deliberately stay
 * outside the shell — only /a/:appId, which requires an actual app to launch
 * into, gets the launcher and frame.
 */
export default function App() {
  const [appsState, setAppsState] = useState<AppsState>({ status: "loading" });

  const loadApps = useCallback(() => {
    setAppsState({ status: "loading" });
    void listApps()
      .then((apps) => setAppsState({ status: "ready", apps }))
      .catch(() => setAppsState({ status: "failed" }));
  }, []);

  useEffect(() => {
    loadApps();
  }, [loadApps]);

  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/request" element={<RequestAccess />} />
        <Route path="/claim" element={<Claim />} />
        <Route path="/account" element={<Account />} />
        <Route path="/admin" element={<Admin />} />
        <Route path="/a/:appId" element={<ShellRoute state={appsState} onRetry={loadApps} />} />
        <Route path="*" element={<Home />} />
      </Routes>
    </BrowserRouter>
  );
}
