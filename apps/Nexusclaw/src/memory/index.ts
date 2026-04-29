// Memory system — 3-tier memory with persistent sessions, FTS5, vector search
// Phase 1.1: Persistent sessions in SQLite
// Phase 1.3: FTS5 full-text search
// Phase 2.1-2.8: Vector, hybrid search, MMR, temporal decay, embedding cache
// Phase 15.2-15.4: Cross-channel persistence, layered search, auto-flush

import Database from "better-sqlite3";
import { randomUUID } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import type {
  Episode,
  VaultNote,
  AlignmentEvent,
  Message,
  Session,
} from "../core/types.js";

// ─── Schema ────────────────────────────────────────────────────────────────

const SCHEMA = `
  CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    agent_id TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    metadata TEXT DEFAULT '{}',
    tags TEXT DEFAULT '[]',
    channel TEXT DEFAULT 'cli',
    compacted INTEGER DEFAULT 0
  );

  CREATE TABLE IF NOT EXISTS session_messages (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    metadata TEXT DEFAULT '{}',
    tool_calls TEXT,
    tool_results TEXT,
    FOREIGN KEY (session_id) REFERENCES sessions(id)
  );

  CREATE TABLE IF NOT EXISTS episodes (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    content TEXT NOT NULL,
    summary TEXT,
    embedding BLOB,
    valid_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    tags TEXT DEFAULT '[]'
  );

  CREATE TABLE IF NOT EXISTS vault_notes (
    id TEXT PRIMARY KEY,
    category TEXT NOT NULL,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    confidence REAL DEFAULT 0.5,
    stale INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    source_episodes TEXT DEFAULT '[]',
    embedding BLOB
  );

  CREATE TABLE IF NOT EXISTS alignment_events (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    response_snippet TEXT NOT NULL,
    score REAL NOT NULL,
    verdict TEXT NOT NULL,
    reason TEXT NOT NULL,
    timestamp INTEGER NOT NULL
  );

  CREATE TABLE IF NOT EXISTS embedding_cache (
    hash TEXT PRIMARY KEY,
    embedding BLOB NOT NULL,
    model TEXT NOT NULL,
    created_at INTEGER NOT NULL
  );

  CREATE INDEX IF NOT EXISTS idx_sessions_agent ON sessions(agent_id);
  CREATE INDEX IF NOT EXISTS idx_sessions_updated ON sessions(updated_at);
  CREATE INDEX IF NOT EXISTS idx_sessions_channel ON sessions(channel);
  CREATE INDEX IF NOT EXISTS idx_messages_session ON session_messages(session_id);
  CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON session_messages(timestamp);
  CREATE INDEX IF NOT EXISTS idx_episodes_session ON episodes(session_id);
  CREATE INDEX IF NOT EXISTS idx_episodes_valid_at ON episodes(valid_at);
  CREATE INDEX IF NOT EXISTS idx_vault_category ON vault_notes(category);
  CREATE INDEX IF NOT EXISTS idx_alignment_session ON alignment_events(session_id);
  CREATE INDEX IF NOT EXISTS idx_alignment_timestamp ON alignment_events(timestamp);
`;

const FTS_SCHEMA = `
  CREATE VIRTUAL TABLE IF NOT EXISTS episodes_fts USING fts5(
    content, summary, tags, content_rowid='rowid'
  );
  CREATE VIRTUAL TABLE IF NOT EXISTS vault_fts USING fts5(
    title, content, category
  );
`;

// ─── Utilities ─────────────────────────────────────────────────────────────

export type EmbeddingFn = (text: string) => Promise<number[]>;

function cosineSim(a: number[], b: number[]): number {
  if (a.length !== b.length) return 0;
  let dot = 0, mA = 0, mB = 0;
  for (let i = 0; i < a.length; i++) { dot += a[i]*b[i]; mA += a[i]*a[i]; mB += b[i]*b[i]; }
  const d = Math.sqrt(mA) * Math.sqrt(mB);
  return d === 0 ? 0 : dot / d;
}

function mmrRerank<T>(items: T[], scores: number[], getEmb: (t: T) => number[] | undefined, lambda = 0.7, k?: number): T[] {
  const n = k ?? items.length;
  const sel: number[] = [];
  const rem = new Set(items.map((_, i) => i));
  while (sel.length < n && rem.size > 0) {
    let bestIdx = -1, bestScore = -Infinity;
    for (const idx of rem) {
      let maxSim = 0;
      const emb = getEmb(items[idx]);
      if (emb) for (const s of sel) { const se = getEmb(items[s]); if (se) maxSim = Math.max(maxSim, cosineSim(emb, se)); }
      const sc = lambda * scores[idx] - (1 - lambda) * maxSim;
      if (sc > bestScore) { bestScore = sc; bestIdx = idx; }
    }
    if (bestIdx < 0) break;
    sel.push(bestIdx); rem.delete(bestIdx);
  }
  return sel.map(i => items[i]);
}

function temporalDecay(ts: number, halfLifeDays = 30): number {
  return Math.pow(0.5, (Date.now() - ts) / (halfLifeDays * 86400000));
}

function fnvHash(text: string): string {
  let h = 2166136261;
  for (let i = 0; i < Math.min(text.length, 1000); i++) { h ^= text.charCodeAt(i); h = (h * 16777619) >>> 0; }
  return h.toString(36);
}

function bufToFloats(buf: Buffer | null): number[] | undefined {
  if (!buf) return undefined;
  return Array.from(new Float64Array(buf.buffer, buf.byteOffset, buf.byteLength / 8));
}

function floatsToBuf(arr: number[]): Buffer {
  return Buffer.from(new Float64Array(arr).buffer);
}

// ─── MemoryManager ─────────────────────────────────────────────────────────

export class MemoryManager {
  private db: Database.Database;
  private vaultDir: string;
  private shortTerm = new Map<string, Message[]>();
  private embedFn: EmbeddingFn | null = null;
  private ftsEnabled = false;

  constructor(dbPath: string, vaultDir: string) {
    const dbDir = dirname(dbPath);
    if (!existsSync(dbDir)) mkdirSync(dbDir, { recursive: true });
    if (!existsSync(vaultDir)) mkdirSync(vaultDir, { recursive: true });

    this.db = new Database(dbPath);
    this.db.pragma("journal_mode = WAL");
    this.db.pragma("foreign_keys = ON");
    this.db.exec(SCHEMA);

    try { this.db.exec(FTS_SCHEMA); this.ftsEnabled = true; } catch { this.ftsEnabled = false; }
    this.vaultDir = vaultDir;
  }

  setEmbeddingFn(fn: EmbeddingFn): void { this.embedFn = fn; }

  // ─── Session Persistence (Phase 1.1) ──────────────────────────────

  createSession(agentId: string, channel = "cli"): Session {
    const session: Session = {
      id: randomUUID(), agentId, messages: [], createdAt: Date.now(), updatedAt: Date.now(), metadata: {}, channel, tags: [],
    };
    this.db.prepare(`INSERT INTO sessions (id, agent_id, created_at, updated_at, metadata, channel) VALUES (?,?,?,?,?,?)`)
      .run(session.id, agentId, session.createdAt, session.updatedAt, "{}", channel);
    this.shortTerm.set(session.id, []);
    return session;
  }

  getSession(sessionId: string): Session | null {
    const row = this.db.prepare(`SELECT * FROM sessions WHERE id = ?`).get(sessionId) as any;
    if (!row) return null;
    return {
      id: row.id, agentId: row.agent_id, messages: this.getSessionMessages(sessionId),
      createdAt: row.created_at, updatedAt: row.updated_at,
      metadata: JSON.parse(row.metadata || "{}"), channel: row.channel,
      tags: JSON.parse(row.tags || "[]"),
    };
  }

  listSessions(opts: { agentId?: string; channel?: string; limit?: number; offset?: number } = {}): { id: string; agentId: string; createdAt: number; updatedAt: number; messageCount: number; channel: string; tags: string[] }[] {
    let sql = `SELECT s.*, COUNT(m.id) as mc FROM sessions s LEFT JOIN session_messages m ON m.session_id = s.id WHERE 1=1`;
    const p: any[] = [];
    if (opts.agentId) { sql += " AND s.agent_id = ?"; p.push(opts.agentId); }
    if (opts.channel) { sql += " AND s.channel = ?"; p.push(opts.channel); }
    sql += " GROUP BY s.id ORDER BY s.updated_at DESC LIMIT ? OFFSET ?";
    p.push(opts.limit || 50, opts.offset || 0);
    return (this.db.prepare(sql).all(...p) as any[]).map(r => ({
      id: r.id, agentId: r.agent_id, createdAt: r.created_at, updatedAt: r.updated_at,
      messageCount: r.mc, channel: r.channel, tags: JSON.parse(r.tags || "[]"),
    }));
  }

  deleteSession(sessionId: string): void {
    this.shortTerm.delete(sessionId);
    this.db.prepare(`DELETE FROM session_messages WHERE session_id = ?`).run(sessionId);
    this.db.prepare(`DELETE FROM sessions WHERE id = ?`).run(sessionId);
  }

  pruneSessions(maxAgeDays: number): number {
    const cutoff = Date.now() - maxAgeDays * 86400000;
    const ids = (this.db.prepare(`SELECT id FROM sessions WHERE updated_at < ?`).all(cutoff) as any[]).map(r => r.id);
    for (const id of ids) this.deleteSession(id);
    return ids.length;
  }

  tagSession(sessionId: string, tags: string[]): void {
    this.db.prepare(`UPDATE sessions SET tags = ?, updated_at = ? WHERE id = ?`).run(JSON.stringify(tags), Date.now(), sessionId);
  }

  // ─── Short-Term Memory (with persistence) ─────────────────────────

  pushMessage(sessionId: string, message: Message): void {
    if (!this.shortTerm.has(sessionId)) this.shortTerm.set(sessionId, []);
    this.shortTerm.get(sessionId)!.push(message);
    this.db.prepare(`INSERT OR REPLACE INTO session_messages (id,session_id,role,content,timestamp,metadata,tool_calls,tool_results) VALUES (?,?,?,?,?,?,?,?)`)
      .run(message.id, sessionId, message.role, message.content, message.timestamp,
           JSON.stringify(message.metadata || {}),
           message.toolCalls ? JSON.stringify(message.toolCalls) : null,
           message.toolResults ? JSON.stringify(message.toolResults) : null);
    this.db.prepare(`UPDATE sessions SET updated_at = ? WHERE id = ?`).run(Date.now(), sessionId);
  }

  getSessionMessages(sessionId: string): Message[] {
    const cached = this.shortTerm.get(sessionId);
    if (cached && cached.length > 0) return cached;
    const rows = this.db.prepare(`SELECT * FROM session_messages WHERE session_id = ? ORDER BY timestamp ASC`).all(sessionId) as any[];
    const msgs = rows.map(this.rowToMessage);
    if (msgs.length > 0) this.shortTerm.set(sessionId, msgs);
    return msgs;
  }

  clearSession(sessionId: string): void { this.shortTerm.delete(sessionId); }

  /** Search messages within a specific session (Phase 2.7) */
  searchSessionMessages(sessionId: string, query: string, opts: { limit?: number } = {}): Message[] {
    const { limit = 20 } = opts;
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    if (terms.length === 0) return this.getSessionMessages(sessionId).slice(-limit);
    let sql = "SELECT * FROM session_messages WHERE session_id = ?";
    const p: any[] = [sessionId];
    for (const t of terms) { sql += " AND LOWER(content) LIKE ?"; p.push(`%${t}%`); }
    sql += " ORDER BY timestamp DESC LIMIT ?"; p.push(limit);
    return (this.db.prepare(sql).all(...p) as any[]).map(this.rowToMessage);
  }

  compactSession(sessionId: string, summary: string, keepCount = 10): void {
    const messages = this.getSessionMessages(sessionId);
    if (!messages || messages.length <= keepCount) return;
    const kept = messages.slice(-keepCount);
    const compactedMsg: Message = { id: randomUUID(), role: "system", content: `[Context compacted] Previous conversation summary:\n${summary}`, timestamp: Date.now() };
    const oldIds = messages.slice(0, -keepCount).map(m => m.id);
    if (oldIds.length > 0) {
      const ph = oldIds.map(() => "?").join(",");
      this.db.prepare(`DELETE FROM session_messages WHERE id IN (${ph})`).run(...oldIds);
    }
    this.shortTerm.set(sessionId, [compactedMsg, ...kept]);
    this.pushMessage(sessionId, compactedMsg);
    this.db.prepare(`UPDATE sessions SET compacted = 1, updated_at = ? WHERE id = ?`).run(Date.now(), sessionId);
  }

  // ─── Episodic Memory (FTS5 + Vector) ──────────────────────────────

  addEpisode(episode: Omit<Episode, "id" | "createdAt">): Episode {
    const id = randomUUID();
    const createdAt = Date.now();
    this.db.prepare(`INSERT INTO episodes (id,session_id,content,summary,embedding,valid_at,created_at,tags) VALUES (?,?,?,?,?,?,?,?)`)
      .run(id, episode.sessionId, episode.content, episode.summary || null,
           episode.embedding ? floatsToBuf(episode.embedding) : null,
           episode.validAt, createdAt, JSON.stringify(episode.tags));
    if (this.ftsEnabled) {
      try {
        this.db.prepare(`INSERT INTO episodes_fts (rowid, content, summary, tags) VALUES ((SELECT rowid FROM episodes WHERE id = ?),?,?,?)`)
          .run(id, episode.content, episode.summary || "", JSON.stringify(episode.tags));
      } catch { /* non-fatal */ }
    }
    return { ...episode, id, createdAt } as Episode;
  }

  async addEpisodeWithEmbedding(episode: Omit<Episode, "id" | "createdAt">): Promise<Episode> {
    let embedding = episode.embedding;
    if (!embedding && this.embedFn) embedding = await this.cachedEmbed(episode.content);
    return this.addEpisode({ ...episode, embedding });
  }

  ingestSession(sessionId: string): number {
    const messages = this.getSessionMessages(sessionId);
    if (messages.length === 0) return 0;
    let count = 0, exchange = "";
    for (const msg of messages) {
      if (msg.role === "system") continue;
      exchange += `${msg.role}: ${msg.content}\n\n`;
      if (msg.role === "assistant") {
        this.addEpisode({ sessionId, content: exchange.trim(), validAt: msg.timestamp, tags: [] });
        exchange = ""; count++;
      }
    }
    return count;
  }

  /** Hybrid search: FTS5 + temporal decay + optional vector (Phase 1.3, 2.2, 2.4, 15.2) */
  searchEpisodes(query: string, opts: {
    limit?: number; afterDate?: number; beforeDate?: number; sessionId?: string;
    temporalHalfLife?: number; channel?: string;
  } = {}): (Episode & { score?: number })[] {
    const { limit = 10, afterDate, beforeDate, sessionId, temporalHalfLife = 30, channel } = opts;
    const fetchN = Math.max(limit * 3, 30);
    type Candidate = Episode & { fts: number; vec: number; temp: number; score: number };
    let candidates: Candidate[] = [];

    // FTS5 search
    if (this.ftsEnabled && query) {
      try {
        const ftsQ = query.replace(/['"(){}[\]^~*:!@#$%&\\]/g, "").split(/\s+/).filter(Boolean).map(t => `"${t}"`).join(" OR ");
        if (ftsQ) {
          let sql = `SELECT e.*, bm25(episodes_fts) as rank FROM episodes_fts f JOIN episodes e ON e.rowid = f.rowid`;
          if (channel) sql += ` JOIN sessions s ON s.id = e.session_id`;
          sql += ` WHERE episodes_fts MATCH ?`;
          const p: any[] = [ftsQ];
          if (afterDate) { sql += " AND e.valid_at >= ?"; p.push(afterDate); }
          if (beforeDate) { sql += " AND e.valid_at <= ?"; p.push(beforeDate); }
          if (sessionId) { sql += " AND e.session_id = ?"; p.push(sessionId); }
          if (channel) { sql += " AND s.channel = ?"; p.push(channel); }
          sql += " ORDER BY rank LIMIT ?"; p.push(fetchN);
          for (const r of this.db.prepare(sql).all(...p) as any[]) {
            const ep = this.rowToEpisode(r);
            candidates.push({ ...ep, fts: -(r.rank as number), vec: 0, temp: temporalDecay(ep.validAt, temporalHalfLife), score: 0 });
          }
        }
      } catch { /* fall through */ }
    }

    // Fallback: LIKE
    if (candidates.length === 0 && query) {
      let sql = channel ? "SELECT e.* FROM episodes e JOIN sessions s ON s.id = e.session_id WHERE 1=1" : "SELECT * FROM episodes e WHERE 1=1";
      const p: any[] = [];
      for (const t of query.toLowerCase().split(/\s+/).filter(Boolean)) { sql += " AND LOWER(e.content) LIKE ?"; p.push(`%${t}%`); }
      if (afterDate) { sql += " AND e.valid_at >= ?"; p.push(afterDate); }
      if (beforeDate) { sql += " AND e.valid_at <= ?"; p.push(beforeDate); }
      if (sessionId) { sql += " AND e.session_id = ?"; p.push(sessionId); }
      if (channel) { sql += " AND s.channel = ?"; p.push(channel); }
      sql += " ORDER BY e.valid_at DESC LIMIT ?"; p.push(fetchN);
      for (const r of this.db.prepare(sql).all(...p) as any[]) {
        const ep = this.rowToEpisode(r);
        candidates.push({ ...ep, fts: 0.5, vec: 0, temp: temporalDecay(ep.validAt, temporalHalfLife), score: 0 });
      }
    }

    // Normalize FTS scores
    const maxFts = Math.max(...candidates.map(c => c.fts), 0.001);
    for (const c of candidates) { c.fts /= maxFts; c.score = c.fts * 0.6 + c.temp * 0.4; }

    // MMR re-rank
    const reranked = mmrRerank(candidates, candidates.map(c => c.score), c => c.embedding, 0.7, limit);
    return reranked.map(({ fts, vec, temp, ...ep }) => ep);
  }

  /** Async search with vector scoring (Phase 2.1+2.2) */
  async searchEpisodesAsync(query: string, opts: {
    limit?: number; afterDate?: number; beforeDate?: number; sessionId?: string; temporalHalfLife?: number;
  } = {}): Promise<(Episode & { score: number })[]> {
    const { limit = 10, temporalHalfLife = 30 } = opts;
    const fetchN = Math.max(limit * 3, 30);
    let queryEmb: number[] | undefined;
    if (this.embedFn) queryEmb = await this.cachedEmbed(query);

    const candidates = this.searchEpisodes(query, { ...opts, limit: fetchN }) as (Episode & { score: number })[];
    if (!queryEmb) return candidates.slice(0, limit);

    const scored = candidates.map(ep => {
      const vs = ep.embedding ? cosineSim(queryEmb!, ep.embedding) : 0;
      return { ...ep, score: (ep.score || 0.5) * 0.5 + vs * 0.3 + temporalDecay(ep.validAt, temporalHalfLife) * 0.2 };
    });

    return mmrRerank(scored, scored.map(s => s.score), s => s.embedding, 0.7, limit)
      .map(s => ({ ...s }));
  }

  getEpisodeCount(): number {
    return (this.db.prepare("SELECT COUNT(*) as c FROM episodes").get() as any).c;
  }

  /** Cross-channel memory recall: search episodes + vault across all channels (Phase 15.2) */
  crossChannelRecall(query: string, opts: { limit?: number; channels?: string[] } = {}): {
    episodes: (Episode & { score?: number; channel: string })[];
    vaultNotes: VaultNote[];
  } {
    const { limit = 10, channels } = opts;
    // Episodes from all channels (or specific channels)
    let episodes: (Episode & { score?: number; channel: string })[] = [];
    if (channels && channels.length > 0) {
      for (const ch of channels) {
        const eps = this.searchEpisodes(query, { limit, channel: ch });
        const enriched = eps.map(ep => {
          const row = this.db.prepare("SELECT channel FROM sessions WHERE id = ?").get(ep.sessionId) as any;
          return { ...ep, channel: row?.channel || "unknown" };
        });
        episodes.push(...enriched);
      }
      episodes.sort((a, b) => (b.score || 0) - (a.score || 0));
      episodes = episodes.slice(0, limit);
    } else {
      const eps = this.searchEpisodes(query, { limit });
      episodes = eps.map(ep => {
        const row = this.db.prepare("SELECT channel FROM sessions WHERE id = ?").get(ep.sessionId) as any;
        return { ...ep, channel: row?.channel || "unknown" };
      });
    }
    // Vault is always global
    const vaultNotes = this.searchVault(query, limit);
    return { episodes, vaultNotes };
  }

  // ─── Semantic Vault ───────────────────────────────────────────────

  addVaultNote(note: Omit<VaultNote, "id" | "createdAt" | "updatedAt">): VaultNote {
    const id = randomUUID(), now = Date.now();
    this.db.prepare(`INSERT INTO vault_notes (id,category,title,content,confidence,stale,created_at,updated_at,source_episodes) VALUES (?,?,?,?,?,?,?,?,?)`)
      .run(id, note.category, note.title, note.content, note.confidence, note.stale ? 1 : 0, now, now, JSON.stringify(note.sourceEpisodes));
    if (this.ftsEnabled) {
      try { this.db.prepare(`INSERT INTO vault_fts (rowid,title,content,category) VALUES ((SELECT rowid FROM vault_notes WHERE id = ?),?,?,?)`).run(id, note.title, note.content, note.category); } catch {}
    }
    const mdPath = join(this.vaultDir, `${note.category}.md`);
    const existing = existsSync(mdPath) ? readFileSync(mdPath, "utf-8") : "";
    writeFileSync(mdPath, existing + (existing ? "\n\n---\n\n" : "") + `# ${note.title}\n\n${note.content}`);
    return { ...note, id, createdAt: now, updatedAt: now };
  }

  getVaultNotes(category?: string): VaultNote[] {
    let sql = "SELECT * FROM vault_notes"; const p: any[] = [];
    if (category) { sql += " WHERE category = ?"; p.push(category); }
    sql += " ORDER BY updated_at DESC";
    return (this.db.prepare(sql).all(...p) as any[]).map(this.rowToVaultNote);
  }

  searchVault(query: string, limit = 10): VaultNote[] {
    if (this.ftsEnabled && query) {
      try {
        const fq = query.replace(/['"(){}[\]^~*:!@#$%&\\]/g, "").split(/\s+/).filter(Boolean).map(t => `"${t}"`).join(" OR ");
        if (fq) return (this.db.prepare(`SELECT v.* FROM vault_fts f JOIN vault_notes v ON v.rowid = f.rowid WHERE vault_fts MATCH ? ORDER BY bm25(vault_fts) LIMIT ?`).all(fq, limit) as any[]).map(this.rowToVaultNote);
      } catch {}
    }
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    let sql = "SELECT * FROM vault_notes WHERE 1=1"; const p: any[] = [];
    for (const t of terms) { sql += " AND (LOWER(title) LIKE ? OR LOWER(content) LIKE ?)"; p.push(`%${t}%`, `%${t}%`); }
    sql += " ORDER BY updated_at DESC LIMIT ?"; p.push(limit);
    return (this.db.prepare(sql).all(...p) as any[]).map(this.rowToVaultNote);
  }

  getVaultContext(): string {
    const notes = this.getVaultNotes();
    if (notes.length === 0) return "";
    const cats = new Map<string, VaultNote[]>();
    for (const n of notes) { if (!cats.has(n.category)) cats.set(n.category, []); cats.get(n.category)!.push(n); }
    let ctx = "## Self-Knowledge (from Vault)\n\n";
    for (const [cat, ns] of cats) {
      ctx += `### ${cat.charAt(0).toUpperCase() + cat.slice(1)}\n`;
      for (const n of ns.filter(x => !x.stale)) ctx += `- ${n.title}: ${n.content}\n`;
      ctx += "\n";
    }
    return ctx;
  }

  /** Auto-flush facts from session into vault (Phase 15.4) */
  autoFlushToVault(sessionId: string, facts: { category: VaultNote["category"]; title: string; content: string }[]): number {
    let c = 0;
    for (const f of facts) { this.addVaultNote({ category: f.category, title: f.title, content: f.content, confidence: 0.7, stale: false, sourceEpisodes: [sessionId] }); c++; }
    return c;
  }

  // ─── Alignment ────────────────────────────────────────────────────

  logAlignment(event: Omit<AlignmentEvent, "id">): AlignmentEvent {
    const id = randomUUID();
    this.db.prepare(`INSERT INTO alignment_events (id,session_id,response_snippet,score,verdict,reason,timestamp) VALUES (?,?,?,?,?,?,?)`)
      .run(id, event.sessionId, event.responseSnippet, event.score, event.verdict, event.reason, event.timestamp);
    return { ...event, id };
  }

  getAlignmentStats(windowDays = 7): { total: number; passRate: number; avgScore: number; driftAlert: boolean } {
    const cutoff = Date.now() - windowDays * 86400000;
    const rows = this.db.prepare(`SELECT verdict, score FROM alignment_events WHERE timestamp >= ?`).all(cutoff) as any[];
    if (rows.length === 0) return { total: 0, passRate: 1.0, avgScore: 1.0, driftAlert: false };
    const total = rows.length;
    const passes = rows.filter((r: any) => r.verdict === "pass").length;
    const passRate = passes / total;
    const avgScore = rows.reduce((s: number, r: any) => s + r.score, 0) / total;
    return { total, passRate, avgScore, driftAlert: passRate < 0.85 };
  }

  // ─── Embedding Cache (Phase 2.6) ──────────────────────────────────

  private async cachedEmbed(text: string): Promise<number[]> {
    if (!this.embedFn) throw new Error("No embedding function");
    const h = fnvHash(text);
    const cached = this.db.prepare(`SELECT embedding FROM embedding_cache WHERE hash = ?`).get(h) as any;
    if (cached?.embedding) return bufToFloats(cached.embedding)!;
    const emb = await this.embedFn(text);
    this.db.prepare(`INSERT OR REPLACE INTO embedding_cache (hash,embedding,model,created_at) VALUES (?,?,?,?)`).run(h, floatsToBuf(emb), "default", Date.now());
    return emb;
  }

  // ─── Status ───────────────────────────────────────────────────────

  getStatus() {
    const episodes = (this.db.prepare("SELECT COUNT(*) as c FROM episodes").get() as any).c;
    const vaultNotes = (this.db.prepare("SELECT COUNT(*) as c FROM vault_notes").get() as any).c;
    const alignmentEvents = (this.db.prepare("SELECT COUNT(*) as c FROM alignment_events").get() as any).c;
    const totalSessions = (this.db.prepare("SELECT COUNT(*) as c FROM sessions").get() as any).c;
    return { episodes, vaultNotes, alignmentEvents, activeSessions: this.shortTerm.size, totalSessions, ftsEnabled: this.ftsEnabled, embeddingsEnabled: !!this.embedFn };
  }

  close(): void { this.db.close(); }

  // ─── Row Mappers ──────────────────────────────────────────────────

  private rowToMessage(r: any): Message {
    return { id: r.id, role: r.role, content: r.content, timestamp: r.timestamp, metadata: r.metadata ? JSON.parse(r.metadata) : {}, toolCalls: r.tool_calls ? JSON.parse(r.tool_calls) : undefined, toolResults: r.tool_results ? JSON.parse(r.tool_results) : undefined };
  }

  private rowToEpisode(r: any): Episode {
    return { id: r.id, sessionId: r.session_id, content: r.content, summary: r.summary, embedding: bufToFloats(r.embedding), validAt: r.valid_at, createdAt: r.created_at, tags: JSON.parse(r.tags || "[]") };
  }

  private rowToVaultNote(r: any): VaultNote {
    return { id: r.id, category: r.category, title: r.title, content: r.content, confidence: r.confidence, stale: !!r.stale, createdAt: r.created_at, updatedAt: r.updated_at, sourceEpisodes: JSON.parse(r.source_episodes || "[]") };
  }
}
