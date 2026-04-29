// Phase 3.8: Canvas / A2UI — Artifact-to-UI
// In-memory artifact store for interactive HTML artifacts
// Artifacts persist for the lifetime of the gateway process

import { randomUUID } from "node:crypto";

export interface Artifact {
  id: string;
  title: string;
  description?: string;
  /** Raw HTML (with optional inline CSS/JS) to render in a sandboxed iframe */
  html: string;
  /** MIME type — always text/html for now */
  contentType: string;
  createdAt: number;
  agentId?: string;
  sessionId?: string;
}

const store = new Map<string, Artifact>();

export function createArtifact(params: {
  title: string;
  html: string;
  description?: string;
  agentId?: string;
  sessionId?: string;
}): Artifact {
  const id = randomUUID();
  const artifact: Artifact = {
    id,
    title: params.title,
    description: params.description,
    html: params.html,
    contentType: "text/html",
    createdAt: Date.now(),
    agentId: params.agentId,
    sessionId: params.sessionId,
  };
  store.set(id, artifact);
  return artifact;
}

export function getArtifact(id: string): Artifact | undefined {
  return store.get(id);
}

export function deleteArtifact(id: string): boolean {
  return store.delete(id);
}

export function listArtifacts(): Omit<Artifact, "html">[] {
  return [...store.values()].map(({ html: _html, ...meta }) => meta).sort(
    (a, b) => b.createdAt - a.createdAt,
  );
}

export function clearArtifacts(): void {
  store.clear();
}
