export type DeploymentModel = "self-hosted" | "federated" | "embedded";

export interface CompanyConfig {
  name: string;
  division: string;
  summary: string;
  primaryHub: string;
}

export interface PlatformConfig {
  name: string;
  vision: string;
  plannedScale: number;
}

export interface RepoConfig {
  id: string;
  name: string;
  repoPath: string;
  category: string;
  deploymentModel: DeploymentModel[];
  federation: boolean;
  embeddedInNexusCloud: boolean;
  publicFacing: boolean;
}

export interface EcosystemConfig {
  company: CompanyConfig;
  platform: PlatformConfig;
  repositories: RepoConfig[];
}
