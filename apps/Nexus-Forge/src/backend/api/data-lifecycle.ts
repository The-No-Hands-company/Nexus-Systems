export function registerDataLifecycleRoutes(app: any) {
  app.get("/api/data/backups", async () => ({ backups: [], note: "Backup catalog is stubbed." }));
  app.post("/api/data/backups/create", async ({ body }: any) => ({ ok: true, backupId: "backup-stub", options: body || {} }));
  app.post("/api/data/backups/restore", async ({ body }: any) => ({ ok: true, restoreId: "restore-stub", options: body || {} }));

  app.get("/api/data/imports", async () => ({ imports: [], note: "Import pipeline is stubbed." }));
  app.post("/api/data/imports", async ({ body }: any) => ({ ok: true, importJobId: "import-stub", source: body || {} }));

  app.get("/api/data/exports", async () => ({ exports: [], note: "Export pipeline is stubbed." }));
  app.post("/api/data/exports", async ({ body }: any) => ({ ok: true, exportJobId: "export-stub", target: body || {} }));

  app.get("/api/data/retention", async () => ({ policies: [], note: "Retention policy management is stubbed." }));
  app.post("/api/data/retention", async ({ body }: any) => ({ ok: true, policy: body || {} }));
}
