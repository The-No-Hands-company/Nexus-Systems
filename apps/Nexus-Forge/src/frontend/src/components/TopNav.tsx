import { Link } from "react-router-dom";

export default function TopNav() {
  return (
    <nav className="top-nav">
      <Link to="/" className="nav-brand">
        <span className="nav-brand-icon">F</span>
        Nexus Forge
      </Link>
      <div className="nav-links">
        <Link to="/" className="nav-link">Repositories</Link>
      </div>
    </nav>
  );
}
