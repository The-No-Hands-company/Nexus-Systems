import { useCallback, useEffect, useState, type ReactNode } from "react";
import { BrowserRouter, Routes, Route, useParams } from "react-router-dom";
import Home from "./pages/Home";
import RequestAccess from "./pages/RequestAccess";
import Claim from "./pages/Claim";
import Account from "./pages/Account";
import Admin from "./pages/Admin";
import MailList from "./pages/mail/MailList";
import MailRead from "./pages/mail/MailRead";
import MailCompose from "./pages/mail/MailCompose";
import CloudOverview from "./pages/cloud/CloudOverview";
import CloudTools from "./pages/cloud/CloudTools";
import CloudFederation from "./pages/cloud/CloudFederation";
import CloudIdentity from "./pages/cloud/CloudIdentity";
import CloudApi from "./pages/cloud/CloudApi";
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
 * A shell-native view: the ecosystem's chrome around a page this app owns,
 * rather than around a framed app.
 *
 * Unlike ShellRoute, the content does not depend on the app list — account
 * settings must render whether or not the launcher could be populated. A
 * failed fetch degrades the sidebar to empty and nothing else. Treating it as
 * fatal here would make an unrelated network failure look like a broken
 * account page.
 */
function ShellView({ state, children }: { state: AppsState; children: ReactNode }) {
  const apps = state.status === "ready" ? state.apps : [];
  return <Shell sidebar={<Launcher apps={apps} />}>{children}</Shell>;
}

/**
 * Plain paths, and the dashboard server serves index.html for any unmatched
 * path, so a hard reload on /claim works rather than 404ing.
 *
 * Three groups, deliberately:
 *
 * - `/request` and `/claim` are public pages for people with no session and no
 *   apps yet. Chrome that advertises a launcher they cannot use would be a
 *   lie, so they stay bare.
 * - `/` stays bare too, but for a different reason: signed in it renders the
 *   launcher grid, and the shell's sidebar is also a launcher. Wrapping it
 *   would put the same four apps on screen twice. The grid is the home
 *   surface; the shell appears when you enter something.
 * - `/account` and `/admin` are signed-in surfaces this app owns, and they get
 *   the shell so they stop reading as separate websites. `/cloud`,
 *   `/cloud/tools`, `/cloud/federation`, `/cloud/identity` and `/cloud/api`
 *   join them: Cloud's operator console, ported in as shell-native views (see
 *   docs/superpowers/specs/2026-08-14-cloud-console-as-shell-views-design.md)
 *   rather than a separate site Cloud serves.
 *
 * Cloud's users view is deliberately absent. Cloud has delegated accounts to
 * Nexus-Auth — its own POST /api/v1/users answers 410 saying so — and its GET
 * requires a Cloud session that SSO no longer issues, so the endpoint returns
 * 401 even to the operator's API key. Porting it would have shipped a view
 * that can never load. A real user list belongs to Auth and is its own work.
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
        <Route
          path="/account"
          element={
            <ShellView state={appsState}>
              <Account />
            </ShellView>
          }
        />
        <Route
          path="/admin"
          element={
            <ShellView state={appsState}>
              <Admin />
            </ShellView>
          }
        />
        <Route
          path="/cloud"
          element={
            <ShellView state={appsState}>
              <CloudOverview />
            </ShellView>
          }
        />
        <Route
          path="/cloud/tools"
          element={
            <ShellView state={appsState}>
              <CloudTools />
            </ShellView>
          }
        />
        <Route
          path="/cloud/federation"
          element={
            <ShellView state={appsState}>
              <CloudFederation />
            </ShellView>
          }
        />
        <Route
          path="/cloud/identity"
          element={
            <ShellView state={appsState}>
              <CloudIdentity />
            </ShellView>
          }
        />
        <Route
          path="/cloud/api"
          element={
            <ShellView state={appsState}>
              <CloudApi />
            </ShellView>
          }
        />
        <Route
          path="/mail"
          element={<ShellView state={appsState}><MailList /></ShellView>}
        />
        <Route
          path="/mail/f/:folderId"
          element={<ShellView state={appsState}><MailList /></ShellView>}
        />
        <Route
          path="/mail/m/:messageId"
          element={<ShellView state={appsState}><MailRead /></ShellView>}
        />
        <Route
          path="/mail/compose"
          element={<ShellView state={appsState}><MailCompose /></ShellView>}
        />
        <Route path="/a/:appId" element={<ShellRoute state={appsState} onRetry={loadApps} />} />
        <Route path="*" element={<Home />} />
      </Routes>
    </BrowserRouter>
  );
}
