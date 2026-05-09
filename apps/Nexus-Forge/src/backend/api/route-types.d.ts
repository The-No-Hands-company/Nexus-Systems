interface ForgeRouteContext {
  body: unknown;
  params: Record<string, string>;
  query: Record<string, string | undefined>;
  headers: Record<string, string | undefined>;
}

interface ForgeRouteApp {
  get(path: string, handler: (context: ForgeRouteContext) => unknown): ForgeRouteApp;
  post(path: string, handler: (context: ForgeRouteContext) => unknown): ForgeRouteApp;
}
