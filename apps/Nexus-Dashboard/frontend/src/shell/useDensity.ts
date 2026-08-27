import { useCallback, useEffect, useState } from "react";

export type Density = "balanced" | "compact";

const KEY = "nexus.density";
const VALID: Density[] = ["balanced", "compact"];

/**
 * Shell-local only. This preference has the same cross-origin problem as the
 * theme — localStorage does not reach chat.tnhc.dev — and both are solved once,
 * in the theming spec, rather than twice and badly.
 */
export function readDensity(): Density {
  const raw = typeof localStorage === "undefined" ? null : localStorage.getItem(KEY);
  return VALID.includes(raw as Density) ? (raw as Density) : "balanced";
}

export function applyDensity(d: Density): void {
  document.documentElement.setAttribute("data-nexus-density", d);
}

export function useDensity(): [Density, (d: Density) => void] {
  const [density, setState] = useState<Density>(readDensity);

  useEffect(() => { applyDensity(density); }, [density]);

  const set = useCallback((d: Density) => {
    localStorage.setItem(KEY, d);
    setState(d);
  }, []);

  return [density, set];
}
