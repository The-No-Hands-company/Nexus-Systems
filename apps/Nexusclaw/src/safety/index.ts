// Safety & Guardrails system for AnyClaw
// Phase 1.4: Interactive confirmation prompts
// Phase 1.6: Rate limiting enforcement
// Phase 9.1: Per-tool cost tracking
// Phase 9.2: Elevated mode
// Phase 9.3-9.6: Sandbox profiles, SSRF policies

import { resolve } from "node:path";
import type { Permissions } from "../core/types.js";

// ─── Sandbox Profiles (Phase 9.6) ──────────────────────────────────────────

export type SandboxProfile = "strict" | "standard" | "permissive";

export interface ProfileOverrides {
  rateLimit: number;
  blockedCommands: string[];
  confirmationRequired: string[];
  deniedTools: string[];
}

const SANDBOX_PROFILES: Record<SandboxProfile, ProfileOverrides> = {
  strict: {
    rateLimit: 30,
    blockedCommands: [
      "rm -rf /", "dd if=", "mkfs", "chmod 777", "curl|sh", "wget|sh",
      "sudo", "su ", "shutdown", "reboot", "kill -9", "> /dev/",
    ],
    confirmationRequired: [
      "git push", "git reset", "docker rm", "docker rmi", "npm publish",
      "rm ", "mv ", "cp ", "write_file", "shell",
    ],
    deniedTools: ["process_spawn", "process_kill"],
  },
  standard: {
    rateLimit: 100,
    blockedCommands: ["rm -rf /", "dd if=", "mkfs"],
    confirmationRequired: ["git push", "docker rm", "docker rmi"],
    deniedTools: [],
  },
  permissive: {
    rateLimit: 300,
    blockedCommands: ["rm -rf /", "dd if=", "mkfs"],
    confirmationRequired: [],
    deniedTools: [],
  },
};

export function getSandboxProfile(profile: SandboxProfile): ProfileOverrides {
  return SANDBOX_PROFILES[profile];
}

export class SafetyManager {
  private permissions: Permissions;
  private actionLog: ActionLogEntry[] = [];
  private pendingConfirmations = new Map<string, ConfirmationRequest>();
  private rateLimitWindow: number[] = [];
  private rateLimit: number;
  private confirmationHandler?: (req: ConfirmationRequest) => Promise<boolean>;

  constructor(permissions: Permissions, options?: { rateLimit?: number }) {
    this.permissions = permissions;
    this.rateLimit = options?.rateLimit ?? 100; // max actions per minute
  }

  // ─── Elevated Mode (Phase 9.2) ────────────────────────────────────

  isElevated(): boolean {
    return this.permissions.elevated === true;
  }

  setElevated(on: boolean): void {
    this.permissions.elevated = on;
  }

  // ─── Confirmation Handler (Phase 1.4) ─────────────────────────────

  setConfirmationHandler(
    handler: (req: ConfirmationRequest) => Promise<boolean>,
  ): void {
    this.confirmationHandler = handler;
  }

  // ─── Path Validation ───────────────────────────────────────────────

  isPathAllowed(filepath: string): boolean {
    if (this.permissions.elevated) return true;
    if (this.permissions.allowedPaths.length === 0) return true;
    const resolved = resolve(filepath);
    return this.permissions.allowedPaths.some((allowed) =>
      resolved.startsWith(resolve(allowed)),
    );
  }

  // ─── Command Validation ────────────────────────────────────────────

  validateCommand(command: string): CommandValidation {
    if (this.permissions.elevated) {
      return { allowed: true, requiresConfirmation: false };
    }

    const blocked = this.permissions.blockedCommands.find((b) =>
      command.toLowerCase().includes(b.toLowerCase()),
    );
    if (blocked) {
      return {
        allowed: false,
        reason: `Command contains blocked pattern: "${blocked}"`,
        requiresConfirmation: false,
      };
    }

    const needsConfirm = this.permissions.confirmationRequired.find((c) =>
      command.toLowerCase().includes(c.toLowerCase()),
    );
    if (needsConfirm) {
      return {
        allowed: true,
        reason: `Requires confirmation: matches "${needsConfirm}"`,
        requiresConfirmation: true,
      };
    }

    return { allowed: true, requiresConfirmation: false };
  }

  // ─── Tool Validation ──────────────────────────────────────────────

  isToolAllowed(toolName: string): boolean {
    if (this.permissions.elevated) return true;
    if (this.permissions.deniedTools.includes(toolName)) return false;
    if (this.permissions.allowedTools.length === 0) return true;
    return this.permissions.allowedTools.includes(toolName);
  }

  // ─── Rate Limiting (Phase 1.6) ────────────────────────────────────

  private checkRateLimit(): { allowed: boolean; remaining: number } {
    const now = Date.now();
    const windowStart = now - 60_000;
    this.rateLimitWindow = this.rateLimitWindow.filter((t) => t > windowStart);
    const remaining = this.rateLimit - this.rateLimitWindow.length;
    if (remaining <= 0) {
      return { allowed: false, remaining: 0 };
    }
    this.rateLimitWindow.push(now);
    return { allowed: true, remaining: remaining - 1 };
  }

  // ─── Confirmation Gates ───────────────────────────────────────────

  requestConfirmation(action: string, details: string): string {
    const id = crypto.randomUUID();
    this.pendingConfirmations.set(id, {
      id,
      action,
      details,
      requestedAt: Date.now(),
      status: "pending",
    });
    return id;
  }

  approveConfirmation(id: string): boolean {
    const req = this.pendingConfirmations.get(id);
    if (!req || req.status !== "pending") return false;
    req.status = "approved";
    return true;
  }

  denyConfirmation(id: string): boolean {
    const req = this.pendingConfirmations.get(id);
    if (!req || req.status !== "pending") return false;
    req.status = "denied";
    return true;
  }

  isConfirmed(id: string): boolean {
    return this.pendingConfirmations.get(id)?.status === "approved";
  }

  async requestInteractiveConfirmation(
    action: string,
    details: string,
  ): Promise<boolean> {
    if (this.permissions.elevated) return true;
    if (!this.confirmationHandler) return false;
    const id = this.requestConfirmation(action, details);
    const req = this.pendingConfirmations.get(id)!;
    const approved = await this.confirmationHandler(req);
    if (approved) this.approveConfirmation(id);
    else this.denyConfirmation(id);
    return approved;
  }

  // ─── Action Logging ───────────────────────────────────────────────

  logAction(entry: Omit<ActionLogEntry, "timestamp">): void {
    this.actionLog.push({ ...entry, timestamp: Date.now() });
    if (this.actionLog.length > 10000) {
      this.actionLog = this.actionLog.slice(-5000);
    }
  }

  getActionLog(limit = 50): ActionLogEntry[] {
    return this.actionLog.slice(-limit);
  }

  // ─── Guardrail Checks ────────────────────────────────────────────

  checkAction(action: AgentAction): GuardrailVerdict {
    const warnings: string[] = [];

    // Rate limit check (Phase 1.6)
    const rateCheck = this.checkRateLimit();
    if (!rateCheck.allowed) {
      return {
        allowed: false,
        reason: "Rate limit exceeded — too many actions per minute",
        warnings,
        requiresConfirmation: false,
      };
    }
    if (rateCheck.remaining < 10) {
      warnings.push(`Rate limit: ${rateCheck.remaining} actions remaining this minute`);
    }

    // Tool permission
    if (!this.isToolAllowed(action.toolName)) {
      return {
        allowed: false,
        reason: `Tool "${action.toolName}" is not permitted`,
        warnings,
        requiresConfirmation: false,
      };
    }

    // File path in arguments
    for (const [key, value] of Object.entries(action.args)) {
      if (
        typeof value === "string" &&
        (key === "path" || key === "cwd" || key === "file") &&
        !this.isPathAllowed(value)
      ) {
        return {
          allowed: false,
          reason: `Path "${value}" is outside allowed directories`,
          warnings,
          requiresConfirmation: false,
        };
      }
    }

    // Shell commands
    if (action.toolName === "shell" && typeof action.args.command === "string") {
      const cmdCheck = this.validateCommand(action.args.command);
      if (!cmdCheck.allowed) {
        return {
          allowed: false,
          reason: cmdCheck.reason || "Command blocked",
          warnings,
          requiresConfirmation: false,
        };
      }
      if (cmdCheck.requiresConfirmation) {
        return {
          allowed: true,
          warnings,
          requiresConfirmation: true,
          confirmationDetails: `Shell command requires confirmation: ${action.args.command}`,
        };
      }
    }

    // High rate warning
    const recentActions = this.actionLog.filter(
      (a) => Date.now() - a.timestamp < 60000,
    );
    if (recentActions.length > 50) {
      warnings.push(
        `High action rate: ${recentActions.length} actions in the last minute`,
      );
    }

    return { allowed: true, warnings, requiresConfirmation: false };
  }

  getPermissions(): Permissions {
    return { ...this.permissions };
  }

  updatePermissions(partial: Partial<Permissions>): void {
    Object.assign(this.permissions, partial);
  }
}

// ─── Types ──────────────────────────────────────────────────────────────────

interface CommandValidation {
  allowed: boolean;
  reason?: string;
  requiresConfirmation: boolean;
}

export interface ConfirmationRequest {
  id: string;
  action: string;
  details: string;
  requestedAt: number;
  status: "pending" | "approved" | "denied";
}

interface ActionLogEntry {
  agentId: string;
  toolName: string;
  args: Record<string, unknown>;
  result?: string;
  error?: string;
  timestamp: number;
}

interface AgentAction {
  agentId: string;
  toolName: string;
  args: Record<string, unknown>;
}

interface GuardrailVerdict {
  allowed: boolean;
  reason?: string;
  warnings: string[];
  requiresConfirmation: boolean;
  confirmationDetails?: string;
}
