import { useCallback, useEffect, useState } from "react";

export type Theme = "void" | "abyss" | "petrol" | "slate";

export const THEMES: { id: Theme; name: string; description: string; hex: string }[] = [
  { id: "void", name: "Void", description: "Near-black minimal canvas", hex: "#030303" },
  { id: "abyss", name: "Abyss", description: "Deep midnight darkness", hex: "#05070D" },
  { id: "petrol", name: "Petrol", description: "Subtle oceanic teal", hex: "#04090A" },
  { id: "slate", name: "Slate", description: "Neutral graphite zinc", hex: "#08090A" },
];

const KEY = "nexus.theme";
const VALID: Theme[] = ["void", "abyss", "petrol", "slate"];

export function readTheme(): Theme {
  const raw = typeof localStorage === "undefined" ? null : localStorage.getItem(KEY);
  return VALID.includes(raw as Theme) ? (raw as Theme) : "void";
}

export function applyTheme(t: Theme): void {
  if (typeof document !== "undefined" && document.documentElement) {
    document.documentElement.setAttribute("data-nexus-theme", t);
  }
}

export function useTheme(): [Theme, (t: Theme) => void] {
  const [theme, setState] = useState<Theme>(readTheme);

  useEffect(() => {
    applyTheme(theme);
  }, [theme]);

  const set = useCallback((t: Theme) => {
    try {
      localStorage.setItem(KEY, t);
    } catch {
      // Storage unavailable or disabled
    }
    setState(t);
  }, []);

  return [theme, set];
}
