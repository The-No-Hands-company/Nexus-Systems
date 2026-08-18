import { NavLink } from "react-router-dom";
import type { MailFolder } from "../../api";

/** Folder order people expect, rather than alphabetical. */
const ORDER = ["inbox", "drafts", "sent", "archive", "trash"];

export default function MailNav({ folders }: { folders: MailFolder[] }) {
  const sorted = [...folders].sort(
    (a, b) => ORDER.indexOf(a.kind) - ORDER.indexOf(b.kind),
  );

  return (
    <nav aria-label="Mail folders" className="flex flex-wrap gap-1 border-b border-zinc-800 pb-3">
      {sorted.map((f) => (
        <NavLink
          key={f.id}
          to={`/mail/f/${f.id}`}
          className={({ isActive }) =>
            `rounded px-3 py-1.5 text-sm ${
              isActive
                ? "bg-zinc-800 text-zinc-100"
                : "text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"
            }`
          }
        >
          {f.name}
        </NavLink>
      ))}
      <NavLink
        to="/mail/compose"
        className="ml-auto rounded bg-zinc-100 px-3 py-1.5 text-sm font-medium text-zinc-900 hover:bg-white"
      >
        Compose
      </NavLink>
    </nav>
  );
}
