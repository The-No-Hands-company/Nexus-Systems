import { readFileSync } from "fs";
import { parse as parseYaml } from "yaml";
import { z } from "zod";
import { logger } from "./logger";

const RouteSchema = z.object({
  upstreams: z.array(
    z.object({
      name: z.string(),
      priority: z.number().optional(),
    })
  ),
  load_balance: z.string().optional().default("round_robin"),
  federation_aware: z.boolean().optional().default(false),
});

const AIProviderRuleSchema = z.object({
  pattern: z.string(),
  model: z.string(),
  fallback: z.string().optional(),
});

const ConfigSchema = z.object({
  router: z.object({
    port: z.number().default(9999),
    hostname: z.string().default("0.0.0.0"),
  }),
  cloud: z.object({
    url: z.string(),
    apiKey: z.string(),
    heartbeat_interval_sec: z.number().default(30),
  }),
  auth: z.object({
    phantom_enabled: z.boolean().default(true),
    jwt_secret: z.string().optional(),
  }),
  geography: z
    .object({
      default_region: z.string(),
      regions: z
        .record(
          z.object({
            cidrs: z.array(z.string()),
            cloud_url: z.string(),
          })
        )
        .optional(),
    })
    .optional(),
  routes: z.record(RouteSchema).optional(),
  ai_providers: z
    .object({
      default_model: z.string(),
      routing_rules: z.array(AIProviderRuleSchema).optional(),
    })
    .optional(),
  telemetry: z
    .object({
      logs: z
        .object({
          enabled: z.boolean(),
          format: z.string().optional(),
          elasticsearch: z
            .object({
              url: z.string(),
            })
            .optional(),
        })
        .optional(),
      metrics: z
        .object({
          enabled: z.boolean(),
          prometheus: z
            .object({
              url: z.string(),
            })
            .optional(),
        })
        .optional(),
      tracing: z
        .object({
          enabled: z.boolean(),
          jaeger: z
            .object({
              url: z.string(),
            })
            .optional(),
        })
        .optional(),
    })
    .optional(),
  federation: z
    .object({
      enabled: z.boolean().default(false),
      peers: z
        .array(
          z.object({
            name: z.string(),
            url: z.string(),
            region: z.string(),
            trust_level: z.string(),
          })
        )
        .optional(),
    })
    .optional(),
});

export type Config = z.infer<typeof ConfigSchema>;

export async function loadConfig(configPath: string): Promise<Config> {
  try {
    const content = readFileSync(configPath, "utf-8");
    const raw = parseYaml(content);
    const config = ConfigSchema.parse(raw);
    logger.info({ config_path: configPath }, "Config loaded and validated");
    return config;
  } catch (err) {
    logger.error(
      {
        config_path: configPath,
        error: err instanceof Error ? err.message : String(err),
      },
      "Failed to load config"
    );
    throw err;
  }
}
