import React from "react";
import { cn } from "./cn";

export type StatTone = "default" | "success" | "warning" | "danger";

export interface StatProps extends React.HTMLAttributes<HTMLDivElement> {
  label: string;
  value: React.ReactNode;
  sub?: string;
  tone?: StatTone;
}

const valueToneClass: Record<StatTone, string> = {
  default: "text-zinc-100",
  success: "text-state-success",
  warning: "text-state-warning",
  danger:  "text-state-danger",
};

const containerToneClass: Record<StatTone, string> = {
  default: "border-zinc-700 bg-zinc-800/40",
  success: "border-zinc-700 bg-zinc-800/40",
  warning: "border-zinc-700 bg-zinc-800/40",
  danger:  "border-state-danger/30 bg-state-danger/5",
};

export const Stat = ({ label, value, sub, tone = "default", className, ...props }: StatProps) => (
  <div
    className={cn(
      "rounded-lg border p-4",
      containerToneClass[tone],
      className,
    )}
    {...props}
  >
    <div className="text-[10px] font-semibold uppercase tracking-widest text-zinc-500">{label}</div>
    <div className={cn("mt-2 text-3xl font-semibold tabular-nums", valueToneClass[tone])}>{value}</div>
    {sub && <div className="mt-0.5 text-xs text-zinc-500">{sub}</div>}
  </div>
);
