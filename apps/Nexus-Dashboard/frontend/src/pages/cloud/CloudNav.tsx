import { NavLink } from "react-router-dom";

const TABS = [
  { to: "/cloud", label: "Overview", end: true },
  { to: "/cloud/tools", label: "Tools" },
  { to: "/cloud/federation", label: "Federation" },
  { to: "/cloud/identity", label: "Identity" },
  { to: "/cloud/api", label: "API" },
] as const;

/**
 * The tab strip across Cloud's six ported views. status.html had these as
 * sidebar nav items inside its own single-page app; here they are ordinary
 * routes, so a small shared strip is what makes them reachable from one
 * another instead of only by typing a URL.
 */
export default function CloudNav() {
  return (
    <nav aria-label="Cloud console" className="flex flex-wrap gap-1 border-b border-zinc-800 pb-3">
      {TABS.map((tab) => (
        <NavLink
          key={tab.to}
          to={tab.to}
          end={"end" in tab ? tab.end : false}
          className={({ isActive }) =>
            `rounded px-3 py-1.5 text-sm ${
              isActive ? "bg-zinc-800 text-zinc-100" : "text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"
            }`
          }
        >
          {tab.label}
        </NavLink>
      ))}
    </nav>
  );
}
