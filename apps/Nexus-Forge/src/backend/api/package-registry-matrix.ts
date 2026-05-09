import { entityResponse, listResponse } from "./contracts";

export function registerPackageRegistryMatrixRoutes(app: ForgeRouteApp) {
  app.get("/api/package-registry-matrix/formats", async () =>
    listResponse(
      [
        "npm",
        "docker-oci",
        "maven",
        "nuget",
        "pypi",
        "rubygems",
        "composer",
        "conan",
        "helm",
        "cargo",
        "go-mod",
      ],
      "Unified package format coverage",
    ),
  );

  app.get("/api/package-registry-matrix/capabilities", async () =>
    listResponse([], "Immutability, retention, access control, and vulnerability-policy stubs"),
  );

  app.post("/api/package-registry-matrix/stub", async ({ body }) =>
    entityResponse(
      { id: "package-matrix-stub", payload: body || {} },
      "Package registry parity stub request accepted",
    ),
  );
}
