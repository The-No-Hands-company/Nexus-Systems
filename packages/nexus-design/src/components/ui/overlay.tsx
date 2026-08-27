import React from "react";
import { cn } from "./cn";

export interface OverlayProps {
  open: boolean;
  onClose: () => void;
  /** Accessible name. A modal with no name is unusable with a screen reader. */
  label: string;
  children: React.ReactNode;
  className?: string;
}

/**
 * The first modal surface in the system.
 *
 * Focus moves in on open and back to the previously focused element on close —
 * without the second half, dismissing the apps drawer drops focus to the body
 * and keyboard navigation restarts from the top of the page.
 */
export function Overlay({ open, onClose, label, children, className }: OverlayProps) {
  const panel = React.useRef<HTMLDivElement>(null);
  const restoreTo = React.useRef<HTMLElement | null>(null);

  // Real callers pass onClose={() => setOpen(false)} — a new function
  // identity on every parent render. Keeping it in the effect's dependency
  // array below would tear the effect down and re-run it on any unrelated
  // re-render while the overlay is open: cleanup yanks focus back to
  // restoreTo (the trigger), setup immediately re-captures activeElement and
  // refocuses the first child, and restoreTo can end up pointing at the
  // wrong element. Depend on [open] alone; route calls through this ref so
  // the handler always invokes the latest onClose without needing it in the
  // dependency array.
  const onCloseRef = React.useRef(onClose);
  React.useEffect(() => {
    onCloseRef.current = onClose;
  });

  React.useEffect(() => {
    if (!open) return;
    restoreTo.current = document.activeElement as HTMLElement | null;

    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") { e.stopPropagation(); onCloseRef.current(); }
    };
    document.addEventListener("keydown", onKey);

    const first = panel.current?.querySelector<HTMLElement>(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])',
    );
    (first ?? panel.current)?.focus();

    return () => {
      document.removeEventListener("keydown", onKey);
      restoreTo.current?.focus();
    };
  }, [open]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-start justify-center p-4 sm:p-8">
      <div
        aria-hidden="true"
        onClick={onClose}
        className="absolute inset-0 bg-zinc-900/70 backdrop-blur-sm"
      />
      <div
        ref={panel}
        role="dialog"
        aria-modal="true"
        aria-label={label}
        tabIndex={-1}
        className={cn(
          "relative w-full max-w-xl rounded-lg border border-zinc-600",
          "bg-zinc-800 shadow-lg outline-none",
          className,
        )}
      >
        {children}
      </div>
    </div>
  );
}
