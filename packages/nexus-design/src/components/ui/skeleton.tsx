import React from "react";
import { cn } from "./cn";

export interface SkeletonProps {
  lines?: number;
  className?: string;
}

/** Loading, announced as such. A silent placeholder is indistinguishable from
 *  an empty result to anyone using a screen reader. */
export function Skeleton({ lines = 3, className }: SkeletonProps) {
  return (
    <div role="status" aria-busy="true" aria-live="polite" className={cn("flex flex-col gap-2 py-2", className)}>
      <span className="sr-only">Loading</span>
      {Array.from({ length: lines }, (_, i) => (
        <span
          key={i}
          data-skeleton-line=""
          aria-hidden="true"
          className="h-3 w-full animate-pulse rounded bg-zinc-700"
          style={{ width: `${100 - i * 12}%` }}
        />
      ))}
    </div>
  );
}
