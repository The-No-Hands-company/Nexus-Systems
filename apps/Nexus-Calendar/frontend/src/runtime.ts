export type CalendarRuntime = {
  basePath: "/" | "/calendar";
  apiBase: "/api/v1/calendar" | "/ipa/calendar";
  publicBase: string;
  /** Origin used for unauthenticated share links when Calendar is in Dashboard. */
  publicOrigin?: string;
  shellContext: boolean;
  publicToken?: string;
};

export type CalendarRuntimeOverrides = Partial<CalendarRuntime>;

declare global {
  interface Window {
    /** Explicit values emitted by a trusted front door, when available. */
    __NEXUS_CALENDAR_RUNTIME__?: CalendarRuntimeOverrides;
    /** Compact marker used by a front door that only needs to identify shell context. */
    __NEXUS_SHELL_CONTEXT__?: boolean | "proxied-app";
  }
}

const standaloneDefaults: CalendarRuntime = {
  basePath: "/",
  apiBase: "/api/v1/calendar",
  publicBase: "/",
  shellContext: false,
};

const shellDefaults: CalendarRuntime = {
  basePath: "/calendar",
  apiBase: "/ipa/calendar",
  publicBase: "/calendar/",
  publicOrigin: "https://calendar.tnhc.dev",
  shellContext: true,
};

function pathnameOnly(pathname: string): string {
  const raw = pathname.trim();
  try {
    // Accepting a full URL is useful for deterministic tests and does not
    // make the hostname part of runtime or authorization decisions.
    const parsed = new URL(raw, "http://nexus.invalid");
    return parsed.pathname || "/";
  } catch {
    const withoutQuery = raw.split(/[?#]/, 1)[0] ?? "/";
    return withoutQuery.startsWith("/") ? withoutQuery : `/${withoutQuery}`;
  }
}

function inferredPublicToken(pathname: string): string | undefined {
  const match = pathname.match(/^\/share\/([^/]+)\/?$/);
  return match?.[1];
}

function readInjectedRuntime(): CalendarRuntimeOverrides | undefined {
  if (typeof window === "undefined") return undefined;
  if (window.__NEXUS_CALENDAR_RUNTIME__) return window.__NEXUS_CALENDAR_RUNTIME__;
  if (typeof window.__NEXUS_SHELL_CONTEXT__ === "boolean") {
    return { shellContext: window.__NEXUS_SHELL_CONTEXT__ };
  }
  if (window.__NEXUS_SHELL_CONTEXT__ === "proxied-app") {
    return { shellContext: true };
  }
  return undefined;
}

/** Resolve routing and API roots without deriving trust from the hostname. */
export function resolveCalendarRuntime(
  pathname = typeof window === "undefined" ? "/" : window.location.pathname,
  injected = readInjectedRuntime(),
): CalendarRuntime {
  const normalizedPath = pathnameOnly(pathname);
  const inferredShell = normalizedPath === "/calendar" || normalizedPath.startsWith("/calendar/");
  const shellContext = injected?.shellContext ?? inferredShell;
  const defaults = shellContext ? shellDefaults : standaloneDefaults;
  const runtime: CalendarRuntime = {
    ...defaults,
    ...injected,
  };

  const publicToken = injected && "publicToken" in injected
    ? injected.publicToken
    : inferredPublicToken(normalizedPath);
  if (publicToken === undefined) return runtime;
  return { ...runtime, publicToken };
}

/** Build a stable public asset path for either the standalone or shell mount. */
export function assetUrl(path: string, runtime: CalendarRuntime): string {
  if (/^(?:[a-z][a-z\d+.-]*:)?\/\//i.test(path)) return path;
  const relativePath = path.replace(/^\.?\/+/, "");
  const base = runtime.publicBase.endsWith("/") ? runtime.publicBase : `${runtime.publicBase}/`;
  return `${base}${relativePath}`;
}
