import path from "node:path";
import type { EcosystemConfig, RepoConfig } from "../types";
import { ensureDir, writeTextFile } from "./fs";
import { companyOverviewTemplate, platformOverviewTemplate } from "../templates/company";
import {
  apiTemplate,
  architectureTemplate,
  developerGuideTemplate,
  operationsTemplate,
  repoOverviewTemplate,
  userGuideTemplate,
} from "../templates/repo";

interface GenerationResult {
  generatedFiles: string[];
}

function repoDocsBase(outputRoot: string, repo: RepoConfig): string {
  return path.join(outputRoot, "tools", repo.id);
}

async function writeRepoDocs(outputRoot: string, repo: RepoConfig): Promise<string[]> {
  const base = repoDocsBase(outputRoot, repo);
  const files: Array<{ rel: string; content: string }> = [
    { rel: "README.md", content: repoOverviewTemplate(repo) },
    { rel: "architecture.md", content: architectureTemplate(repo) },
    { rel: "developer-guide.md", content: developerGuideTemplate(repo) },
    { rel: "user-guide.md", content: userGuideTemplate(repo) },
    { rel: "operations-runbook.md", content: operationsTemplate(repo) },
    { rel: "api-contracts.md", content: apiTemplate(repo) },
  ];

  await ensureDir(base);

  const written: string[] = [];
  for (const file of files) {
    const fullPath = path.join(base, file.rel);
    await writeTextFile(fullPath, file.content);
    written.push(fullPath);
  }

  return written;
}

function indexTemplate(config: EcosystemConfig): string {
  const tableRows = config.repositories
    .map(
      (repo) =>
        `| ${repo.name} | ${repo.category} | ${repo.deploymentModel.join(", ")} | ${repo.federation ? "Yes" : "No"} | ${repo.embeddedInNexusCloud ? "Yes" : "No"} |`,
    )
    .join("\n");

  return `# Nexus Systems Documentation Index\n\n## Company\n\n- [The No Hands Company](./company/README.md)\n\n## Platform\n\n- [Nexus Systems Overview](./platform/README.md)\n\n## Tool Documentation\n\n| Tool | Category | Deployment | Federated | Embedded In Nexus-Cloud |
| --- | --- | --- | --- | --- |
${tableRows}\n\n## Notes\n\nThis index is generated from src/config/ecosystem.config.json.\n`;
}

export async function generateDocs(config: EcosystemConfig, outputRoot: string): Promise<GenerationResult> {
  await ensureDir(outputRoot);

  const generatedFiles: string[] = [];

  const companyPath = path.join(outputRoot, "company", "README.md");
  const platformPath = path.join(outputRoot, "platform", "README.md");
  const indexPath = path.join(outputRoot, "README.md");

  await writeTextFile(companyPath, companyOverviewTemplate(config.company, config.platform));
  generatedFiles.push(companyPath);

  await writeTextFile(platformPath, platformOverviewTemplate(config.platform));
  generatedFiles.push(platformPath);

  await writeTextFile(indexPath, indexTemplate(config));
  generatedFiles.push(indexPath);

  for (const repo of config.repositories) {
    const repoFiles = await writeRepoDocs(outputRoot, repo);
    generatedFiles.push(...repoFiles);
  }

  return { generatedFiles };
}
