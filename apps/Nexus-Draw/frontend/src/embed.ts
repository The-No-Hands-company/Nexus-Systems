/**
 * Whether this app is being rendered inside the ecosystem shell.
 *
 * A query parameter rather than postMessage or a header: it works identically
 * from any language and any framework, and an app that ignores it still
 * functions — just with its own chrome as well as the shell's. Degrading to
 * "slightly wrong" beats degrading to "blank".
 */
export function isEmbedded(search: string = window.location.search): boolean {
  return new URLSearchParams(search).get("embed") === "1";
}
