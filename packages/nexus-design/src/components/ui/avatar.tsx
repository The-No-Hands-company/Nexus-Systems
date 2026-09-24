import React from "react";
import { cn } from "./cn";

/** Up to two initials, and never an empty circle. */
export function initials(label: string): string {
  const words = label.trim().split(/\s+/).filter(Boolean);
  if (words.length === 0) return "?";
  if (words.length === 1) return words[0]!.slice(0, 2).toUpperCase();
  return (words[0]![0]! + words[1]![0]!).toUpperCase();
}

export interface AvatarProps extends React.HTMLAttributes<HTMLSpanElement> {
  label: string;
}

export function Avatar({ label, className, ...props }: AvatarProps) {
  return (
    <span
      role="img"
      aria-label={label}
      className={cn(
        "inline-flex h-7 w-7 items-center justify-center rounded-full",
        "border border-zinc-600 font-mono text-xs text-zinc-300",
        className,
      )}
      {...props}
    >
      {initials(label)}
    </span>
  );
}
