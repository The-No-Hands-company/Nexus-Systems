import type { CompanyConfig, PlatformConfig } from "../types";

export function companyOverviewTemplate(company: CompanyConfig, platform: PlatformConfig): string {
  return `# ${company.name} Documentation\n\n## Division\n\n${company.division}\n\n## Company Summary\n\n${company.summary}\n\n## Platform Context\n\n${platform.name}\n\n## Primary Hub\n\n${company.primaryHub}\n\n## Documentation Ownership\n\n- Product Owner:\n- Engineering Owner:\n- Documentation Owner:\n\n## Review Cadence\n\n- Weekly engineering docs update\n- Monthly architecture validation\n- Quarterly policy and governance refresh\n`;
}

export function platformOverviewTemplate(platform: PlatformConfig): string {
  return `# ${platform.name} Overview\n\n## Vision\n\n${platform.vision}\n\n## Planned Scale\n\nTarget ecosystem size: ${platform.plannedScale}+ tools\n\n## Operating Modes\n\n- Self-hosted\n- Federated\n- Embedded in Nexus-Cloud\n\n## System Principles\n\n- Modular by default\n- Shared standards for security and observability\n- Clear contracts between services and tools\n\n## Open Questions\n\n- Which standards are globally mandatory?\n- Which tools are public-facing vs internal-only?\n- What are tier-1 uptime and support targets?\n`;
}
