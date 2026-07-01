export interface UsageInfo {
  tokensUsed: number;
  dollarsSpent: number;
  balanceRemainingTokens?: number;
  balanceRemainingDollars?: number;
}

export abstract class BaseProvider {
  constructor(protected config: { apiKey: string; accountId: string }) {}

  abstract getName(): string;
  abstract getProviderId(): string;
  abstract getUsage(): Promise<UsageInfo>;
}
