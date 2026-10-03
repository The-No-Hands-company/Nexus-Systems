import { Link } from "react-router-dom";

export default function TopNav() {
  return (
    <nav className="top-nav">
      <div className="brand">
        <Link to="/">Nexus Forge</Link>
      </div>
      <div className="nav-links">
        <Link to="/">Repositories</Link>
      </div>
    </nav>
  );
}
