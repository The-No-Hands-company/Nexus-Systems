import React from "react";
import { cn } from "./cn";

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "primary" | "secondary" | "outline" | "ghost";
  size?: "sm" | "md" | "lg";
}

/**
 * Colours come from Tailwind utilities that resolve to ecosystem tokens through
 * each app's index.css alias. A hex literal here would pin the button to one
 * theme and silently ignore the other three.
 */
export const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant = "primary", size = "md", ...props }, ref) => {
    const variants = {
      primary: "bg-accent text-accent-foreground hover:bg-accent-hover active:bg-accent-active",
      secondary: "bg-zinc-700 text-zinc-100 hover:bg-zinc-600",
      outline: "border border-zinc-600 text-zinc-100 hover:bg-zinc-800",
      ghost: "text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800",
    };
    const sizes = {
      sm: "px-3 py-1.5 text-xs",
      md: "px-4 py-2 text-sm",
      lg: "px-6 py-3 text-base",
    };
    return (
      <button
        ref={ref}
        className={cn(
          "inline-flex items-center justify-center rounded-md font-medium transition-colors",
          "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent",
          "disabled:pointer-events-none disabled:opacity-50",
          variants[variant],
          sizes[size],
          className,
        )}
        {...props}
      />
    );
  },
);
Button.displayName = "Button";
