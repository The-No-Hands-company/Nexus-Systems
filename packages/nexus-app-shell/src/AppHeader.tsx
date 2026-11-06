import React, { type ReactNode } from "react";

export type AppHeaderProps = {
  homeHref: string;
  appName: string;
  userSlot?: ReactNode;
  utilitySlot?: ReactNode;
  leadingSlot?: ReactNode;
  /**
   * True when the app is framed by the Nexus shell (`?embed=1`). The shell
   * already provides the wordmark and the way home, so repeating them here
   * spends 56px of a phone screen on a second header and offers a "home" link
   * that, inside the frame, only reloads the app.
   */
  embedded?: boolean;
};

/**
 * The small, router-independent header shared by independently served apps.
 *
 * The home link intentionally is an ordinary anchor. App artifacts can be
 * served from their own origin or under Dashboard without sharing router
 * context, while callers retain ownership of identity, utility, and role
 * decisions through slots.
 */
export function AppHeader({
  homeHref,
  appName,
  userSlot,
  utilitySlot,
  leadingSlot,
  embedded = false,
}: AppHeaderProps) {
  return (
    <header
      role="banner"
      className={`flex shrink-0 items-center gap-3 border-b border-white/10 px-3 sm:px-4 ${embedded ? "h-11" : "h-14"}`}
    >
      {leadingSlot}
      {!embedded && (
        <a
          href={homeHref}
          className="flex min-h-11 items-center gap-1.5 font-semibold tracking-tight hover:opacity-90"
          aria-label="Nexus home"
        >
          <svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true" className="text-[#ccff00] shrink-0">
            <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z" />
          </svg>
          <span>Nexus</span>
        </a>
      )}
      <span className="text-sm text-white/60" aria-label={`${appName} app`}>
        {appName}
      </span>
      {utilitySlot && <div className="ml-auto flex items-center gap-2">{utilitySlot}</div>}
      {userSlot && <div className="flex items-center gap-2">{userSlot}</div>}
    </header>
  );
}

export default AppHeader;
