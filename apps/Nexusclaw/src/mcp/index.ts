// AnyClaw — MCP Client Integration (Phase 4)
// Connects to MCP servers via stdio/SSE, discovers tools, registers in ToolRegistry

import { spawn, type ChildProcess } from "node:child_process";
import { createInterface } from "node:readline";
import type { MCPServerConfig, Tool, ToolResult, ToolParameter } from "../core/types.js";

// JSON-RPC types
interface JsonRpcRequest {
  jsonrpc: "2.0";
  id: number;
  method: string;
  params?: Record<string, unknown>;
}

interface JsonRpcResponse {
  jsonrpc: "2.0";
  id: number;
  result?: any;
  error?: { code: number; message: string; data?: any };
}

// ─── MCP Transport Abstraction ──────────────────────────────────────────

interface MCPTransport {
  send(request: JsonRpcRequest): Promise<JsonRpcResponse>;
  close(): Promise<void>;
}

// ─── Stdio Transport ────────────────────────────────────────────────────

class StdioTransport implements MCPTransport {
  private process: ChildProcess;
  private nextId = 1;
  private pending = new Map<number, { resolve: (v: JsonRpcResponse) => void; reject: (e: Error) => void }>();
  private rl: ReturnType<typeof createInterface>;

  constructor(command: string, args: string[], env?: Record<string, string>) {
    this.process = spawn(command, args, {
      stdio: ["pipe", "pipe", "pipe"],
      env: { ...process.env, ...env },
    });

    this.rl = createInterface({ input: this.process.stdout! });
    this.rl.on("line", (line) => {
      try {
        const msg = JSON.parse(line) as JsonRpcResponse;
        if (msg.id !== undefined && this.pending.has(msg.id)) {
          const p = this.pending.get(msg.id)!;
          this.pending.delete(msg.id);
          p.resolve(msg);
        }
      } catch { /* ignore non-JSON lines */ }
    });

    this.process.on("error", (err) => {
      for (const [, p] of this.pending) {
        p.reject(err);
      }
      this.pending.clear();
    });
  }

  async send(request: JsonRpcRequest): Promise<JsonRpcResponse> {
    const id = this.nextId++;
    const req = { ...request, id };

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`MCP request timed out: ${request.method}`));
      }, 30_000);

      this.pending.set(id, {
        resolve: (v) => { clearTimeout(timeout); resolve(v); },
        reject: (e) => { clearTimeout(timeout); reject(e); },
      });

      this.process.stdin!.write(JSON.stringify(req) + "\n");
    });
  }

  async close(): Promise<void> {
    this.rl.close();
    this.process.kill("SIGTERM");
    // Give 2s to exit gracefully
    await new Promise<void>((resolve) => {
      const timer = setTimeout(() => {
        this.process.kill("SIGKILL");
        resolve();
      }, 2000);
      this.process.on("exit", () => { clearTimeout(timer); resolve(); });
    });
  }
}

// ─── SSE Transport ──────────────────────────────────────────────────────

class SSETransport implements MCPTransport {
  private url: string;
  private nextId = 1;

  constructor(url: string) {
    this.url = url;
  }

  async send(request: JsonRpcRequest): Promise<JsonRpcResponse> {
    const id = this.nextId++;
    const req = { ...request, id };

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 30_000);

    try {
      const res = await fetch(this.url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(req),
        signal: controller.signal,
      });

      if (!res.ok) {
        throw new Error(`MCP SSE error: ${res.status} ${res.statusText}`);
      }

      return await res.json() as JsonRpcResponse;
    } finally {
      clearTimeout(timeout);
    }
  }

  async close(): Promise<void> {
    // No persistent connection to close for HTTP-based SSE
  }
}

// ─── MCP Client ─────────────────────────────────────────────────────────

interface MCPToolInfo {
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
}

export interface MCPResource {
  uri: string;
  name: string;
  description?: string;
  mimeType?: string;
}

export interface MCPResourceContent {
  uri: string;
  mimeType?: string;
  text?: string;
  blob?: string; // base64 encoded
}

export interface MCPPromptInfo {
  name: string;
  description?: string;
  arguments?: { name: string; description?: string; required?: boolean }[];
}

export interface MCPPromptMessage {
  role: "user" | "assistant";
  content: { type: string; text?: string }; 
}

export class MCPClient {
  private serverId: string;
  private transport: MCPTransport;
  private initialized = false;

  constructor(serverId: string, transport: MCPTransport) {
    this.serverId = serverId;
    this.transport = transport;
  }

  async initialize(): Promise<void> {
    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "initialize",
      params: {
        protocolVersion: "2024-11-05",
        capabilities: {},
        clientInfo: { name: "anyclaw", version: "0.1.0" },
      },
    });

    if (result.error) {
      throw new Error(`MCP init failed for ${this.serverId}: ${result.error.message}`);
    }

    // Send initialized notification (no response expected)
    this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "notifications/initialized",
    }).catch(() => {});

    this.initialized = true;
  }

  async listTools(): Promise<MCPToolInfo[]> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "tools/list",
    });

    if (result.error) {
      throw new Error(`MCP tools/list failed: ${result.error.message}`);
    }

    return (result.result?.tools || []) as MCPToolInfo[];
  }

  async callTool(name: string, args: Record<string, unknown>): Promise<string> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "tools/call",
      params: { name, arguments: args },
    });

    if (result.error) {
      throw new Error(`MCP tool error (${name}): ${result.error.message}`);
    }

    // MCP returns content as array of text/image blocks
    const content = result.result?.content;
    if (Array.isArray(content)) {
      return content
        .filter((c: any) => c.type === "text")
        .map((c: any) => c.text)
        .join("\n");
    }

    return String(content ?? "");
  }

  async close(): Promise<void> {
    await this.transport.close();
  }

  // ─── Resources (Phase 4.4) ──────────────────────────────────────────

  async listResources(): Promise<MCPResource[]> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "resources/list",
    });

    if (result.error) {
      throw new Error(`MCP resources/list failed: ${result.error.message}`);
    }

    return (result.result?.resources || []) as MCPResource[];
  }

  async readResource(uri: string): Promise<MCPResourceContent[]> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "resources/read",
      params: { uri },
    });

    if (result.error) {
      throw new Error(`MCP resources/read failed: ${result.error.message}`);
    }

    return (result.result?.contents || []) as MCPResourceContent[];
  }

  // ─── Prompts (Phase 4.4) ────────────────────────────────────────────

  async listPrompts(): Promise<MCPPromptInfo[]> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "prompts/list",
    });

    if (result.error) {
      throw new Error(`MCP prompts/list failed: ${result.error.message}`);
    }

    return (result.result?.prompts || []) as MCPPromptInfo[];
  }

  async getPrompt(name: string, args?: Record<string, string>): Promise<MCPPromptMessage[]> {
    if (!this.initialized) await this.initialize();

    const result = await this.transport.send({
      jsonrpc: "2.0",
      id: 0,
      method: "prompts/get",
      params: { name, arguments: args },
    });

    if (result.error) {
      throw new Error(`MCP prompts/get failed: ${result.error.message}`);
    }

    return (result.result?.messages || []) as MCPPromptMessage[];
  }
}

// ─── MCP Manager ────────────────────────────────────────────────────────

export class MCPManager {
  private clients = new Map<string, MCPClient>();

  async connectServer(id: string, config: MCPServerConfig): Promise<MCPClient> {
    if (!config.enabled) {
      throw new Error(`MCP server "${id}" is disabled`);
    }

    let transport: MCPTransport;

    if (config.transport === "sse" && config.url) {
      transport = new SSETransport(config.url);
    } else if (config.command) {
      transport = new StdioTransport(config.command, config.args || [], config.env);
    } else {
      throw new Error(`MCP server "${id}" has no command or URL configured`);
    }

    const client = new MCPClient(id, transport);
    await client.initialize();
    this.clients.set(id, client);
    return client;
  }

  getClient(id: string): MCPClient | undefined {
    return this.clients.get(id);
  }

  /**
   * Discover tools from an MCP server and return them as AnyClaw Tools
   */
  async discoverTools(serverId: string): Promise<Tool[]> {
    const client = this.clients.get(serverId);
    if (!client) throw new Error(`MCP server "${serverId}" not connected`);

    const mcpTools = await client.listTools();

    return mcpTools.map((mt) => {
      // Convert JSON Schema to ToolParameter[]
      const parameters: ToolParameter[] = [];
      const schema = mt.inputSchema as any;
      if (schema?.properties) {
        const required = new Set<string>(schema.required || []);
        for (const [name, prop] of Object.entries<any>(schema.properties)) {
          parameters.push({
            name,
            type: (prop.type || "string") as ToolParameter["type"],
            description: prop.description || "",
            required: required.has(name),
          });
        }
      }

      return {
        name: `mcp_${serverId}_${mt.name}`,
        description: `[MCP:${serverId}] ${mt.description}`,
        parameters,
        execute: async (params: Record<string, unknown>): Promise<unknown> => {
          const output = await client.callTool(mt.name, params);
          return output;
        },
      } satisfies Tool;
    });
  }

  /**
   * Discover resources from an MCP server and return as AnyClaw Tools
   */
  async discoverResources(serverId: string): Promise<Tool[]> {
    const client = this.clients.get(serverId);
    if (!client) throw new Error(`MCP server "${serverId}" not connected`);

    let resources: MCPResource[];
    try {
      resources = await client.listResources();
    } catch {
      return []; // Server may not support resources
    }

    return resources.map((r) => ({
      name: `mcp_${serverId}_resource_${r.name.replace(/[^a-zA-Z0-9_]/g, "_")}`,
      description: `[MCP:${serverId} resource] ${r.description || r.name} (${r.uri})`,
      parameters: [],
      execute: async (): Promise<unknown> => {
        const contents = await client.readResource(r.uri);
        return contents.map((c) => c.text ?? `[binary: ${c.mimeType}]`).join("\n");
      },
    } satisfies Tool));
  }

  /**
   * Connect all servers from config and discover their tools
   */
  async connectAll(
    servers: Record<string, MCPServerConfig>,
  ): Promise<{ tools: Tool[]; errors: { id: string; error: string }[] }> {
    const allTools: Tool[] = [];
    const errors: { id: string; error: string }[] = [];

    for (const [id, config] of Object.entries(servers)) {
      if (!config.enabled) continue;

      try {
        await this.connectServer(id, config);
        const tools = await this.discoverTools(id);
        allTools.push(...tools);
        // Also discover resources as tools
        const resourceTools = await this.discoverResources(id);
        allTools.push(...resourceTools);
      } catch (err: any) {
        errors.push({ id, error: err.message });
      }
    }

    return { tools: allTools, errors };
  }

  async closeAll(): Promise<void> {
    for (const client of this.clients.values()) {
      await client.close().catch(() => {});
    }
    this.clients.clear();
  }
}
