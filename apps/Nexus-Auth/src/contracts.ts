export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: {
    authVersion: string;
    supportsServiceTokens: boolean;
    supportsSeedIdentities: boolean;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-auth",
    name: "Nexus Auth",
    description: "Identity and service authentication provider",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["identity", "service-auth", "token-validation"],
    metadata: {
      authVersion: "v1",
      supportsServiceTokens: true,
      supportsSeedIdentities: true,
    },
  };
}
