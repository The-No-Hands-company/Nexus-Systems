import { IncomingMessage, ServerResponse } from "http";
import { Config } from "../lib/config";
import { logger } from "../lib/logger";

interface RequestContext {
  requestId: string;
  metadata: Record<string, unknown>;
}

/**
 * AI Provider Router: Route to Ollama, Claude, Groq, etc based on complexity and model preference
 */
export async function aiProviderRouter(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config
): Promise<void> {
  try {
    const url = req.url || "/";

    // Only route AI provider requests
    if (!url.includes("/api/ai") && !url.includes("/api/chat")) {
      return; // Not an AI request
    }

    const aiConfig = config.ai_providers;
    if (!aiConfig) {
      return; // No AI provider config
    }

    // Extract task/pattern from request (simplified)
    // In production, parse request body to determine complexity
    const taskPattern = extractTaskPattern(req.url);
    const selectedModel = selectModel(taskPattern, aiConfig);

    context.metadata.ai_provider = selectedModel;

    logger.debug(
      {
        request_id: context.requestId,
        task_pattern: taskPattern,
        selected_model: selectedModel,
      },
      "AI provider router: selected model"
    );

    // Add model selection to headers for upstream
    req.headers["x-ai-model"] = selectedModel;
  } catch (err) {
    logger.warn(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "AI provider router error (continuing)"
    );
  }
}

function extractTaskPattern(url: string): string {
  // Very simplified: extract task type from URL
  // In production, parse request body and headers
  if (url.includes("legal")) return "legal:*";
  if (url.includes("code")) return "code:*";
  return "*";
}

function selectModel(pattern: string, aiConfig: any): string {
  const rules = aiConfig.routing_rules || [];

  for (const rule of rules) {
    if (matchesPattern(pattern, rule.pattern)) {
      return rule.model || aiConfig.default_model || "ollama";
    }
  }

  return aiConfig.default_model || "ollama";
}

function matchesPattern(taskPattern: string, rulePattern: string): boolean {
  // Simple glob matching
  if (rulePattern === "*") return true;
  if (rulePattern === taskPattern) return true;
  if (rulePattern.endsWith(":*")) {
    const prefix = rulePattern.slice(0, -2);
    return taskPattern.startsWith(prefix);
  }
  return false;
}
