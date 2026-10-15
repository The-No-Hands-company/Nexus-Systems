import React from "react";
import { cn } from "./cn";

export type BadgeVariant = "default" | "success" | "warning" | "danger" | "info" | "accent";

export interface BadgeProps extends React.HTMLAttributes<HTMLSpanElement> {
  variant?: BadgeVariant;
  dot?: boolean;
}

const variantClasses: Record<BadgeVariant, string> = {
  default: "bg-zinc-800 text-zinc-300 border border-zinc-700",
  success: "bg-state-success/10 text-state-success border border-state-success/20",
  warning: "bg-state-warning/10 text-state-warning border border-state-warning/20",
  danger:  "bg-state-danger/10  text-state-danger  border border-state-danger/20",
  info:    "bg-state-info/10    text-state-info    border border-state-info/20",
  accent:  "bg-accent/10        text-accent        border border-accent/20",
};

const dotClasses: Record<BadgeVariant, string> = {
  default: "bg-zinc-400",
  success: "bg-state-success",
  warning: "bg-state-warning",
  danger:  "bg-state-danger",
  info:    "bg-state-info",
  accent:  "bg-accent",
};

export const Badge = React.forwardRef<HTMLSpanElement, BadgeProps>(
  ({ className, variant = "default", dot = false, children, ...props }, ref) => (
    <span
      ref={ref}
      className={cn(
        "inline-flex items-center gap-1.5 rounded-full px-2 py-0.5 text-xs font-medium",
        variantClasses[variant],
        className,
      )}
      {...props}
    >
      {dot && <span className={cn("h-1.5 w-1.5 shrink-0 rounded-full", dotClasses[variant])} />}
      {children}
    </span>
  ),
);
Badge.displayName = "Badge";
