import { BaseProvider, UsageInfo } from "./base";

export class AnthropicProvider extends BaseProvider {
  getName(): string {
    return "Anthropic";
  }

  getProviderId(): string {
    return "anthropic";
  }

  async getUsage(): Promise<UsageInfo> {
    // Anthropic doesn't have a public usage API endpoint that's easily accessible via API key
    // usually it's in the console. We'll return 0 for now or placeholder.
    return {
      tokensUsed: 0,
      dollarsSpent: 0,
    };
  }
}
