import React from "react";
import { cn } from "./cn";

export type PillTone = "neutral" | "success" | "warning" | "danger" | "info";

export interface PillProps extends React.HTMLAttributes<HTMLSpanElement> {
  tone?: PillTone;
}

/** A status chip. The dot carries the state colour so the label stays legible
 *  at 3:1 rather than tinting text that has to meet 4.5:1. */
export const Pill = React.forwardRef<HTMLSpanElement, PillProps>(
  ({ className, tone = "neutral", children, ...props }, ref) => {
    const dot = {
      neutral: "bg-zinc-500",
      success: "bg-state-success",
      warning: "bg-state-warning",
      danger: "bg-state-danger",
      info: "bg-state-info",
    }[tone];
    return (
      <span
        ref={ref}
        className={cn(
          "inline-flex items-center gap-1.5 rounded-full border border-zinc-600",
          "px-2 py-0.5 text-xs text-zinc-300",
          className,
        )}
        {...props}
      >
        <span aria-hidden="true" className={cn("h-1.5 w-1.5 rounded-full", dot)} />
        {children}
      </span>
    );
  },
);
Pill.displayName = "Pill";
