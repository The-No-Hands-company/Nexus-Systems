import { BaseProvider, UsageInfo } from "./base";

export class OpenRouterProvider extends BaseProvider {
  getName(): string {
    return "OpenRouter";
  }

  getProviderId(): string {
    return "openrouter";
  }

  async getUsage(): Promise<UsageInfo> {
    const response = await fetch("https://openrouter.ai/api/v1/credits", {
      headers: {
        Authorization: `Bearer ${this.config.apiKey}`,
      },
    });

    if (!response.ok) {
      throw new Error(`Failed to fetch OpenRouter credits: ${response.statusText}`);
    }

    const data = (await response.json()) as any;
    // OpenRouter returns credits in dollars
    return {
      tokensUsed: 0, // OpenRouter doesn't give a total token count easily here
      dollarsSpent: 0, // Need to calculate from total - remaining if possible
      balanceRemainingDollars: data.data.credits,
    };
  }
}
