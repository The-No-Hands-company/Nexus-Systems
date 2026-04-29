export function registerFederationRoutes(app: any) {
  app.get("/.well-known/nexus-cloud", () => ({
    service: "nexus-forge",
    version: "0.1.0",
    endpoints: {
      discovery: "/api/cloud/discovery",
      register: "/api/cloud/register",
      client: "/api/cloud/client",
    },
  }));

  app.get("/api/cloud/discovery", async () => {
    return {
      peers: [],
      message: "Federation discovery is stubbed. Add peer sync later.",
    };
  });

  app.post("/api/cloud/register", async ({ body }: any) => {
    const payload = body as { publicKey?: string; domain?: string; port?: number };
    return {
      ok: true,
      registered: !!payload.domain,
      nodeId: "forge-node-stub",
    };
  });

  app.get("/api/cloud/client", async ({ query }: any) => {
    return {
      endpoint: "http://localhost:8090",
      requestedRepo: query.repo || null,
    };
  });
}

export const federationEndpoints = {
  wellKnown: () => ({
    service: "nexus-forge",
    version: "0.1.0",
    endpoints: {
      discovery: "/api/cloud/discovery",
      register: "/api/cloud/register",
      client: "/api/cloud/client",
    },
  }),
};
