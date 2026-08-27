import React from "react";
import { cn } from "./cn";

export interface EmptyStateProps {
  title: string;
  hint?: string;
  action?: React.ReactNode;
  className?: string;
}

/** "Nothing here" — distinct from "we could not ask", which is an error state. */
export function EmptyState({ title, hint, action, className }: EmptyStateProps) {
  return (
    <div className={cn("flex flex-col items-start gap-1 py-6 text-sm", className)}>
      <p className="text-zinc-300">{title}</p>
      {hint ? <p className="text-zinc-500">{hint}</p> : null}
      {action ? <div className="pt-2">{action}</div> : null}
    </div>
  );
}
