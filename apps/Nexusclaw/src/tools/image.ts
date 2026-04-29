// Image analysis tool — uses multi-modal LLM vision capabilities
// Phase 3.6

import { readFileSync, existsSync } from "node:fs";
import { join, resolve, isAbsolute, extname } from "node:path";
import type { Tool, ToolContext } from "../core/types.js";
import type { LLMRouter } from "../llm/index.js";

const SUPPORTED_EXTENSIONS = new Set([".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg"]);
const MAX_IMAGE_SIZE = 20 * 1024 * 1024; // 20MB

function getMediaType(ext: string): string {
  const types: Record<string, string> = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".bmp": "image/bmp",
    ".svg": "image/svg+xml",
  };
  return types[ext] || "image/png";
}

export function createImageTool(llm: LLMRouter): Tool {
  return {
    name: "image_analyze",
    description:
      "Analyze an image file using AI vision. Returns a detailed text description including objects, text, colors, layout, and context. Supports PNG, JPG, GIF, WebP.",
    parameters: [
      { name: "path", type: "string", description: "Path to the image file", required: true },
      {
        name: "question",
        type: "string",
        description: "Specific question about the image (default: general description)",
        required: false,
      },
    ],
    async execute(args, context) {
      const filepath = isAbsolute(args.path as string)
        ? (args.path as string)
        : join(context.workingDir, args.path as string);

      if (!existsSync(filepath)) throw new Error(`Image not found: ${filepath}`);

      const ext = extname(filepath).toLowerCase();
      if (!SUPPORTED_EXTENSIONS.has(ext)) {
        throw new Error(`Unsupported image format "${ext}". Supported: ${[...SUPPORTED_EXTENSIONS].join(", ")}`);
      }

      const buf = readFileSync(filepath);
      if (buf.length > MAX_IMAGE_SIZE) {
        throw new Error(`Image too large (${(buf.length / 1024 / 1024).toFixed(1)}MB). Max: 20MB`);
      }

      const base64 = buf.toString("base64");
      const mediaType = getMediaType(ext);
      const question = (args.question as string) || "Describe this image in detail. Include any text, objects, colors, layout, and relevant context.";

      // Use Anthropic vision API format
      const response = await llm.chat({
        model: "anthropic/claude-sonnet-4-20250514",
        messages: [
          {
            role: "user",
            content: [
              {
                type: "text",
                text: question,
              },
              {
                type: "text",
                text: `[Image: data:${mediaType};base64,${base64}]`,
              },
            ] as any,
          },
        ],
        maxTokens: 2048,
      });

      const description = response.content
        .filter(b => b.type === "text")
        .map(b => b.text)
        .join("");

      return {
        path: filepath,
        size: buf.length,
        format: ext.slice(1),
        description,
        tokens: response.usage,
      };
    },
  };
}
