import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface GraphicProject {
  id: string;
  name: string;
  type: "vector" | "raster" | "mixed";
  width: number;
  height: number;
  layers: Array<{ name: string; visible: boolean; elements: unknown[] }>;
  createdAt: string;
  updatedAt: string;
}
type GraphicProjectRow = Omit<GraphicProject, "layers" | "createdAt" | "updatedAt"> & {
  layers: string;
  created_at: string;
  updated_at: string;
};

export class GraphicEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS projects (id TEXT PRIMARY KEY, name TEXT, type TEXT, width INTEGER, height INTEGER, layers TEXT DEFAULT '[]', created_at TEXT, updated_at TEXT)",
    );
  }
  createProject(name: string, w = 1920, h = 1080): GraphicProject {
    const p: GraphicProject = {
      id: randomUUID(),
      name,
      type: "vector",
      width: w,
      height: h,
      layers: [{ name: "Background", visible: true, elements: [] }],
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO projects VALUES (?,?,?,?,?,?,?,?)")
      .run(
        p.id,
        p.name,
        p.type,
        p.width,
        p.height,
        JSON.stringify(p.layers),
        p.createdAt,
        p.updatedAt,
      );
    return p;
  }
  listProjects(): GraphicProject[] {
    const rows = this.db
      .prepare("SELECT * FROM projects ORDER BY updated_at DESC")
      .all() as unknown as GraphicProjectRow[];
    return rows.map((r) => ({
      id: r.id,
      name: r.name,
      type: r.type,
      width: r.width,
      height: r.height,
      layers: JSON.parse(r.layers) as GraphicProject["layers"],
      createdAt: r.created_at,
      updatedAt: r.updated_at,
    }));
  }
  getProject(id: string): GraphicProject | undefined {
    const r = this.db
      .prepare("SELECT * FROM projects WHERE id = ?")
      .get(id) as unknown as GraphicProjectRow | null;
    return r
      ? {
          id: r.id,
          name: r.name,
          type: r.type,
          width: r.width,
          height: r.height,
          layers: JSON.parse(r.layers) as GraphicProject["layers"],
          createdAt: r.created_at,
          updatedAt: r.updated_at,
        }
      : undefined;
  }
}
