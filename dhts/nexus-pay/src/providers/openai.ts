import { BaseProvider, UsageInfo } from "./base";

export class OpenAIProvider extends BaseProvider {
  getName(): string {
    return "OpenAI";
  }

  getProviderId(): string {
    return "openai";
  }

  async getUsage(): Promise<UsageInfo> {
    // OpenAI usage API is a bit complex as it's date-based
    // For now, we'll try to get the subscription/billing info
    const response = await fetch("https://api.openai.com/v1/dashboard/billing/usage", {
      headers: {
        Authorization: `Bearer ${this.config.apiKey}`,
      },
    });

    if (!response.ok) {
      // Some endpoints are restricted to session keys, so we might need to fallback to a simpler check
      // or just report that we can't fetch automated billing for this key type yet.
      return {
        tokensUsed: 0,
        dollarsSpent: 0,
      };
    }

    const data = (await response.json()) as any;
    return {
      tokensUsed: data.total_usage || 0,
      dollarsSpent: (data.total_usage || 0) / 100, // Very rough estimate
    };
  }
}
