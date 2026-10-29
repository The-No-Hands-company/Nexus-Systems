import { Link } from "react-router-dom";
import type { ReactNode } from "react";
import type { AppEntry } from "../api";

/**
 * How a launcher tile opens its app.
 *
 * An `external` app is a full standalone SPA on its own origin; it opens in a new
 * tab so it never runs a second runtime inside this one (see
 * docs/architecture/frontend-load-and-app-delivery.md). Everything else routes
 * in-shell to the app's path.
 */
export default function AppLaunchLink({
  app,
  className,
  children,
  "aria-label": ariaLabel,
  "aria-current": ariaCurrent,
}: {
  app: AppEntry;
  className?: string;
  children: ReactNode;
  "aria-label"?: string;
  "aria-current"?: "page" | undefined;
}) {
  if (app.delivery === "external" && app.publicUrl) {
    return (
      <a
        href={app.publicUrl}
        target="_blank"
        rel="noopener noreferrer"
        aria-label={ariaLabel}
        className={className}
      >
        {children}
      </a>
    );
  }
  return (
    <Link to={app.path} aria-label={ariaLabel} aria-current={ariaCurrent} className={className}>
      {children}
    </Link>
  );
}
