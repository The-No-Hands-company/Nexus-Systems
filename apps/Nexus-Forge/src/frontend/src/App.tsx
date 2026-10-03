import { BrowserRouter, Route, Routes } from "react-router-dom";
import TopNav from "./components/TopNav";
import RepoDetail from "./pages/RepoDetail";
import RepoList from "./pages/RepoList";

// Only pages backed by real API routes exist. The ~150 placeholder "*Hub"
// pages were removed on 2026-10-03.
export default function App() {
  return (
    <BrowserRouter>
      <div className="app-shell">
        <TopNav />
        <main className="app-content">
          <Routes>
            <Route path="/" element={<RepoList />} />
            <Route path="/repos/:name" element={<RepoDetail />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  );
}
