import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface DesignProject {
  id: string;
  name: string;
  type: "web" | "mobile" | "desktop" | "other";
  frames: string[];
  collaborators: string[];
  status: "draft" | "in_progress" | "completed";
  createdAt: string;
}

const PROJECT_TYPES = ["web", "mobile", "desktop", "other"] as const;
type DesignProjectRow = Omit<DesignProject, "frames" | "collaborators" | "createdAt" | "type"> & {
  type: string;
  frames: string;
  collaborators: string;
  created_at: string;
};

function projectType(value: string): DesignProject["type"] {
  return (PROJECT_TYPES as readonly string[]).includes(value)
    ? (value as DesignProject["type"])
    : "web";
}

function stringArray(value: string): string[] {
  const parsed: unknown = JSON.parse(value);
  return Array.isArray(parsed) && parsed.every((item) => typeof item === "string") ? parsed : [];
}

export class DesignEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS projects (id TEXT PRIMARY KEY, name TEXT, type TEXT, frames TEXT DEFAULT '[]', collaborators TEXT DEFAULT '[]', status TEXT DEFAULT 'draft', created_at TEXT)",
    );
  }
  createProject(name: string, type: DesignProject["type"] = "web"): DesignProject {
    const p: DesignProject = {
      id: randomUUID(),
      name,
      type: projectType(type),
      frames: [],
      collaborators: [],
      status: "draft",
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO projects VALUES (?,?,?,?,?,?,?)")
      .run(
        p.id,
        p.name,
        p.type,
        JSON.stringify(p.frames),
        JSON.stringify(p.collaborators),
        p.status,
        p.createdAt,
      );
    return p;
  }
  listProjects(): DesignProject[] {
    const rows = this.db
      .prepare("SELECT * FROM projects ORDER BY created_at DESC")
      .all() as unknown as DesignProjectRow[];
    return rows.map((r) => ({
      id: r.id,
      name: r.name,
      type: projectType(r.type),
      frames: stringArray(r.frames),
      collaborators: stringArray(r.collaborators),
      status: r.status,
      createdAt: r.created_at,
    }));
  }
  getProject(id: string): DesignProject | undefined {
    const r = this.db
      .prepare("SELECT * FROM projects WHERE id = ?")
      .get(id) as unknown as DesignProjectRow | null;
    return r
      ? {
          id: r.id,
          name: r.name,
          type: projectType(r.type),
          frames: stringArray(r.frames),
          collaborators: stringArray(r.collaborators),
          status: r.status,
          createdAt: r.created_at,
        }
      : undefined;
  }
  addFrame(projectId: string, frameName: string): void {
    const p = this.getProject(projectId);
    if (!p) return;
    p.frames.push(frameName);
    this.db
      .prepare("UPDATE projects SET frames = ? WHERE id = ?")
      .run(JSON.stringify(p.frames), projectId);
  }
}
