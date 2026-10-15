import React from "react";
import { cn } from "./cn";

export interface PageHeaderProps extends React.HTMLAttributes<HTMLDivElement> {
  title: string;
  description?: string;
  actions?: React.ReactNode;
  nav?: React.ReactNode;
}

export const PageHeader = ({
  title,
  description,
  actions,
  nav,
  className,
  ...props
}: PageHeaderProps) => (
  <div className={cn("border-b border-zinc-700 px-6 pt-6", nav ? "pb-0" : "pb-5", className)} {...props}>
    <div className="flex flex-wrap items-start justify-between gap-4">
      <div>
        <h1 className="text-xl font-semibold tracking-tight text-zinc-100">{title}</h1>
        {description && <p className="mt-1 text-sm text-zinc-400">{description}</p>}
      </div>
      {actions && <div className="flex shrink-0 items-center gap-2">{actions}</div>}
    </div>
    {nav && <div className="mt-4">{nav}</div>}
  </div>
);
