import type { AppEntry } from "../api";

/**
 * Shared by the launcher and the frame so the two cannot disagree about what
 * an app is. Returns undefined rather than throwing: an unknown id arrives
 * from the URL bar, which is user input, not a programming error.
 */
export function appById(apps: AppEntry[], id: string): AppEntry | undefined {
  return apps.find((a) => a.id === id);
}
