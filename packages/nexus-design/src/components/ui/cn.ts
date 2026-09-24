import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

/** The one class-composition helper. Duplicating it per component is how three
 *  copies drifted apart before this file existed. */
export function cn(...inputs: ClassValue[]): string {
  return twMerge(clsx(inputs));
}
