import React from "react";
import { cn } from "./cn";

/** A keyboard hint. Presentational only — the shortcut itself is bound by
 *  whatever renders this. */
export function Kbd({ className, children, ...props }: React.HTMLAttributes<HTMLElement>) {
  return (
    <kbd
      className={cn(
        "inline-flex min-w-5 items-center justify-center rounded border border-zinc-600",
        "bg-zinc-800 px-1.5 py-0.5 font-mono text-xs text-zinc-400",
        className,
      )}
      {...props}
    >
      {children}
    </kbd>
  );
}
