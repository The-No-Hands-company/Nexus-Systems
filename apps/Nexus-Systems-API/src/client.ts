import type {
  PhantomComplianceSummary,
  SystemsApiStatusResponse,
  SystemsApiToolHeartbeat,
  SystemsApiToolRegistration,
} from "./contracts";
import { systemsApiV1Endpoints } from "./contracts";

type ClientOptions = {
  baseUrl: string;
  token?: string;
};

export class NexusSystemsApiClient {
  private readonly baseUrl: string;
  private readonly token?: string;

  constructor(options: ClientOptions) {
    this.baseUrl = options.baseUrl.replace(/\/$/, "");
    this.token = options.token;
  }

  async registerTool(payload: SystemsApiToolRegistration): Promise<Response> {
    return this.request(systemsApiV1Endpoints.registerTool, {
      method: "POST",
      body: JSON.stringify(payload),
    });
  }

  async heartbeat(
    toolId: string,
    payload: Omit<SystemsApiToolHeartbeat, "toolId">,
  ): Promise<Response> {
    const path = systemsApiV1Endpoints.heartbeat.replace(":toolId", encodeURIComponent(toolId));
    return this.request(path, {
      method: "POST",
      body: JSON.stringify({ ...payload, toolId }),
    });
  }

  async getStatus(): Promise<SystemsApiStatusResponse> {
    const response = await this.request(systemsApiV1Endpoints.status, { method: "GET" });
    return (await response.json()) as SystemsApiStatusResponse;
  }

  async getComplianceSummary(): Promise<PhantomComplianceSummary> {
    const response = await this.request(systemsApiV1Endpoints.phantomComplianceSummary, {
      method: "GET",
    });
    return (await response.json()) as PhantomComplianceSummary;
  }

  private request(path: string, init: RequestInit): Promise<Response> {
    const headers = new Headers(init.headers || {});
    headers.set("content-type", "application/json");
    if (this.token) {
      headers.set("authorization", `Bearer ${this.token}`);
    }

    return fetch(`${this.baseUrl}${path}`, {
      ...init,
      headers,
    });
  }
}
