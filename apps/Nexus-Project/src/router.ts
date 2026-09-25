export type Params = Record<string, string>;
export type Handler<C> = (ctx: C, params: Params) => Promise<Response> | Response;
export type Match<C> = { handler: Handler<C>; params: Params } | { allowed: string[] } | null;

interface Route<C> {
  method: string;
  parts: string[];
  handler: Handler<C>;
}

/** Method + path-pattern routing. `:name` segments become decoded params. */
export class Router<C> {
  private readonly routes: Route<C>[] = [];

  add(method: string, pattern: string, handler: Handler<C>): void {
    this.routes.push({ method, parts: split(pattern), handler });
  }

  match(method: string, path: string): Match<C> {
    const parts = split(path);
    const allowed: string[] = [];
    for (const route of this.routes) {
      const params = matchParts(route.parts, parts);
      if (!params) continue;
      if (route.method === method) return { handler: route.handler, params };
      allowed.push(route.method);
    }
    return allowed.length > 0 ? { allowed } : null;
  }
}

/** A route parameter the pattern guarantees exists. */
export function param(params: Params, name: string): string {
  const value = params[name];
  if (value === undefined) throw new Error(`route has no :${name} parameter`);
  return value;
}

function split(path: string): string[] {
  return path.split("/").filter(Boolean);
}

function matchParts(pattern: string[], parts: string[]): Params | null {
  if (pattern.length !== parts.length) return null;
  const params: Params = {};
  for (let i = 0; i < pattern.length; i++) {
    const expected = pattern[i] as string;
    const actual = parts[i] as string;
    if (expected.startsWith(":")) {
      try {
        params[expected.slice(1)] = decodeURIComponent(actual);
      } catch {
        return null;
      }
    } else if (expected !== actual) {
      return null;
    }
  }
  return params;
}
