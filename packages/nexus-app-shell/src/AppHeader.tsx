import React, { type ReactNode } from "react";

export type AppHeaderProps = {
  homeHref: string;
  appName: string;
  userSlot?: ReactNode;
  utilitySlot?: ReactNode;
  leadingSlot?: ReactNode;
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
}: AppHeaderProps) {
  return (
    <header
      role="banner"
      className="flex h-14 shrink-0 items-center gap-3 border-b border-white/10 px-4"
    >
      {leadingSlot}
      <a
        href={homeHref}
        className="font-semibold tracking-tight hover:text-[#ccff00]"
        aria-label="Nexus home"
      >
        Nexus
      </a>
      <span className="text-sm text-white/60" aria-label={`${appName} app`}>
        {appName}
      </span>
      {utilitySlot && <div className="ml-auto flex items-center gap-2">{utilitySlot}</div>}
      {userSlot && <div className="flex items-center gap-2">{userSlot}</div>}
    </header>
  );
}

export default AppHeader;
