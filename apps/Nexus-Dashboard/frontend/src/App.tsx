import { BrowserRouter, Routes, Route } from "react-router-dom";
import Home from "./pages/Home";
import RequestAccess from "./pages/RequestAccess";
import Claim from "./pages/Claim";
import Account from "./pages/Account";

/**
 * Plain paths, and the dashboard server serves index.html for any unmatched
 * path, so a hard reload on /claim works rather than 404ing.
 *
 * Home decides between the app grid and the signed-out landing; account and
 * admin surfaces land in the next tasks.
 */
export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/request" element={<RequestAccess />} />
        <Route path="/claim" element={<Claim />} />
        <Route path="/account" element={<Account />} />
        <Route path="*" element={<Home />} />
      </Routes>
    </BrowserRouter>
  );
}
