export function generateIssuePR(issueId: string, title: string, description: string) {
  return {
    ok: true,
    prTitle: `[ai-generated] ${title}`,
    description: `Stub PR generated from issue ${issueId}: ${description}`,
    warnings: ["AI generation is currently a stub. Review before merging."],
  };
}
