import { useEffect, useState } from "react";
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

/** Renders a single ecosystem app inside the shell's chrome, keyed off the URL. */
function ShellRoute({ apps }: { apps: AppEntry[] }) {
  const { appId = "" } = useParams();
  return (
    <Shell sidebar={<Launcher apps={apps} activeId={appId} />}>
      <AppFrame apps={apps} appId={appId} />
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
  const [apps, setApps] = useState<AppEntry[]>([]);

  useEffect(() => {
    void listApps().then(setApps).catch(() => setApps([]));
  }, []);

  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/request" element={<RequestAccess />} />
        <Route path="/claim" element={<Claim />} />
        <Route path="/account" element={<Account />} />
        <Route path="/admin" element={<Admin />} />
        <Route path="/a/:appId" element={<ShellRoute apps={apps} />} />
        <Route path="*" element={<Home />} />
      </Routes>
    </BrowserRouter>
  );
}
