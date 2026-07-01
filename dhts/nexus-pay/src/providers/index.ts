import { OpenRouterProvider } from "./openrouter";
import { OpenAIProvider } from "./openai";
import { AnthropicProvider } from "./anthropic";
import { BaseProvider } from "./base";

export const ProviderRegistry: Record<string, new (config: any) => BaseProvider> = {
  openrouter: OpenRouterProvider,
  openai: OpenAIProvider,
  anthropic: AnthropicProvider,
  // Add more as implemented
};

export function createProvider(type: string, config: any): BaseProvider {
  const ProviderClass = ProviderRegistry[type];
  if (!ProviderClass) {
    throw new Error(`Unknown provider type: ${type}`);
  }
  return new ProviderClass(config);
}
