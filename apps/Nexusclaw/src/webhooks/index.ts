// ─── Nexusclaw Webhook Dispatcher ─────────────────────────────────────────────
// Phase 5: Outbound HTTP webhook system. Allows external systems to subscribe to
// Nexusclaw events (agent activity, tool calls, session start/end, errors).
// Webhooks are stored in ~/.anyclaw/data/webhooks.json and dispatched via HTTP
// POST with HMAC-SHA256 signatures for verification.

import * as fs from "fs";
import * as path from "path";
import * as os from "os";
import * as crypto from "crypto";

// ─── Types ──────────────────────────────────────────────────────────────────

export type WebhookEvent =
  | "agent.started"
  | "agent.stopped"
  | "agent.error"
  | "tool.called"
  | "tool.failed"
  | "session.created"
  | "session.ended"
  | "chat.message"
  | "gsd.task_updated"
  | "safety.blocked"
  | "*"; // wildcard — all events

export interface WebhookEntry {
  id: string;
  url: string;
  events: WebhookEvent[];
  secret?: string; // HMAC secret for signature header
  enabled: boolean;
  description?: string;
  createdAt: number;
  lastDeliveryAt?: number;
  lastDeliveryStatus?: number;
  failCount: number;
}

export interface WebhookPayload {
  id: string;         // unique delivery ID
  event: WebhookEvent;
  ts: number;
  source: string;     // "nexusclaw"
  data: Record<string, unknown>;
}

export interface DeliveryResult {
  webhookId: string;
  url: string;
  status: number | null;
  ok: boolean;
  durationMs: number;
  error?: string;
}

// ─── WebhookDispatcher ────────────────────────────────────────────────────────

export class WebhookDispatcher {
  private hooks: Map<string, WebhookEntry> = new Map();
  private storeFile: string | undefined;
  private enabled = false;
  private maxFailCount = 10; // disable hook after 10 consecutive failures

  enable(): void { this.enabled = true; }

  setFile(filePath: string): void {
    this.storeFile = filePath;
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
    this.enabled = true;
    this._load();
  }

  // ─── CRUD ─────────────────────────────────────────────────────────────────

  register(opts: {
    url: string;
    events: WebhookEvent[];
    secret?: string;
    description?: string;
  }): WebhookEntry {
    const entry: WebhookEntry = {
      id: `wh_${Date.now().toString(36)}_${crypto.randomBytes(4).toString("hex")}`,
      url: opts.url,
      events: opts.events,
      secret: opts.secret,
      enabled: true,
      description: opts.description,
      createdAt: Date.now(),
      failCount: 0,
    };
    this.hooks.set(entry.id, entry);
    this._save();
    return entry;
  }

  unregister(id: string): boolean {
    const removed = this.hooks.delete(id);
    if (removed) this._save();
    return removed;
  }

  get(id: string): WebhookEntry | undefined {
    return this.hooks.get(id);
  }

  list(): WebhookEntry[] {
    return [...this.hooks.values()];
  }

  enable_hook(id: string): boolean {
    const h = this.hooks.get(id);
    if (!h) return false;
    h.enabled = true;
    h.failCount = 0;
    this._save();
    return true;
  }

  disable_hook(id: string): boolean {
    const h = this.hooks.get(id);
    if (!h) return false;
    h.enabled = false;
    this._save();
    return true;
  }

  // ─── Dispatch ─────────────────────────────────────────────────────────────

  async dispatch(event: WebhookEvent, data: Record<string, unknown>): Promise<DeliveryResult[]> {
    if (!this.enabled || this.hooks.size === 0) return [];
    const payload: WebhookPayload = {
      id: `del_${Date.now().toString(36)}_${crypto.randomBytes(4).toString("hex")}`,
      event,
      ts: Date.now(),
      source: "nexusclaw",
      data,
    };
    const body = JSON.stringify(payload);

    const targets = [...this.hooks.values()].filter(h =>
      h.enabled &&
      h.failCount < this.maxFailCount &&
      (h.events.includes("*") || h.events.includes(event))
    );

    const results = await Promise.all(targets.map(h => this._deliver(h, body, payload)));
    return results;
  }

  // Convenience: fire-and-forget dispatch (does not await or throw)
  emit(event: WebhookEvent, data: Record<string, unknown>): void {
    this.dispatch(event, data).catch(() => {});
  }

  // ─── Internal ─────────────────────────────────────────────────────────────

  private async _deliver(hook: WebhookEntry, body: string, payload: WebhookPayload): Promise<DeliveryResult> {
    const start = Date.now();
    const headers: Record<string, string> = {
      "content-type": "application/json",
      "user-agent": "Nexusclaw-Webhook/1.0",
      "x-nexusclaw-event": payload.event,
      "x-nexusclaw-delivery": payload.id,
    };
    if (hook.secret) {
      const sig = crypto.createHmac("sha256", hook.secret).update(body).digest("hex");
      headers["x-nexusclaw-signature-256"] = `sha256=${sig}`;
    }
    try {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 10_000);
      const res = await fetch(hook.url, {
        method: "POST",
        headers,
        body,
        signal: controller.signal,
      });
      clearTimeout(timeout);
      const durationMs = Date.now() - start;
      hook.lastDeliveryAt = Date.now();
      hook.lastDeliveryStatus = res.status;
      if (res.ok) {
        hook.failCount = 0;
      } else {
        hook.failCount++;
        if (hook.failCount >= this.maxFailCount) hook.enabled = false;
      }
      this._save();
      return { webhookId: hook.id, url: hook.url, status: res.status, ok: res.ok, durationMs };
    } catch (err: unknown) {
      const durationMs = Date.now() - start;
      hook.failCount++;
      hook.lastDeliveryAt = Date.now();
      hook.lastDeliveryStatus = 0;
      if (hook.failCount >= this.maxFailCount) hook.enabled = false;
      this._save();
      const error = err instanceof Error ? err.message : String(err);
      return { webhookId: hook.id, url: hook.url, status: null, ok: false, durationMs, error };
    }
  }

  private _save(): void {
    if (!this.storeFile) return;
    try {
      const data = [...this.hooks.values()];
      fs.writeFileSync(this.storeFile, JSON.stringify(data, null, 2));
    } catch { /* never crash */ }
  }

  private _load(): void {
    if (!this.storeFile || !fs.existsSync(this.storeFile)) return;
    try {
      const data = JSON.parse(fs.readFileSync(this.storeFile, "utf8")) as WebhookEntry[];
      for (const h of data) this.hooks.set(h.id, h);
    } catch { /* corrupt file, start fresh */ }
  }
}

// ─── Singleton ────────────────────────────────────────────────────────────────

export const webhooks = new WebhookDispatcher();

export function defaultWebhooksFile(): string {
  return path.join(os.homedir(), ".anyclaw", "data", "webhooks.json");
}
