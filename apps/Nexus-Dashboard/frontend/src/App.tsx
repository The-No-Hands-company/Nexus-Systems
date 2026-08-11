import { BrowserRouter, Routes, Route, Link } from "react-router-dom";
import RequestAccess from "./pages/RequestAccess";
import Claim from "./pages/Claim";

/**
 * Routes are plain paths, and the dashboard server serves index.html for any
 * unmatched path, so a hard reload on /claim works rather than 404ing.
 *
 * The grid, account and admin surfaces land in later tasks; Home is a
 * deliberately small placeholder rather than a fake dashboard.
 */
function Home() {
  return (
    <section className="mx-auto max-w-xl p-8">
      <h1 className="text-2xl font-semibold">Nexus</h1>
      <p className="mt-2 text-zinc-400">
        One account for every app in the ecosystem.
      </p>
      <div className="mt-6 flex gap-3">
        <Link to="/request" className="rounded bg-blue-600 px-4 py-2 font-medium">
          Request access
        </Link>
        <Link to="/claim" className="rounded border border-zinc-700 px-4 py-2">
          Claim your account
        </Link>
      </div>
    </section>
  );
}

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/request" element={<RequestAccess />} />
        <Route path="/claim" element={<Claim />} />
        <Route path="*" element={<Home />} />
      </Routes>
    </BrowserRouter>
  );
}
