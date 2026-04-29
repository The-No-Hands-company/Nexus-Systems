// PDF text extraction tool
// Phase 3.7 — extracts text from PDF files using CLI tools or basic parsing

import { execSync } from "node:child_process";
import { readFileSync, existsSync } from "node:fs";
import { join, isAbsolute, extname } from "node:path";
import type { Tool, ToolContext } from "../core/types.js";

const MAX_PDF_SIZE = 50 * 1024 * 1024; // 50MB

/** Try pdftotext (poppler-utils), then basic extraction */
function extractPdfText(filepath: string, pages?: string): string {
  // Try pdftotext CLI (best quality)
  try {
    const pageArg = pages ? `-f ${pages.split("-")[0]} -l ${pages.split("-")[1] || pages.split("-")[0]}` : "";
    const output = execSync(`pdftotext ${pageArg} -layout ${JSON.stringify(filepath)} -`, {
      encoding: "utf-8",
      maxBuffer: 10 * 1024 * 1024,
      timeout: 30000,
    });
    return output;
  } catch {
    // pdftotext not available
  }

  // Try mutool (mupdf)
  try {
    const pageArg = pages || "1-N";
    const output = execSync(`mutool draw -F txt -o - ${JSON.stringify(filepath)} ${pageArg}`, {
      encoding: "utf-8",
      maxBuffer: 10 * 1024 * 1024,
      timeout: 30000,
    });
    return output;
  } catch {
    // mutool not available
  }

  // Fallback: basic text extraction from raw PDF bytes
  const buf = readFileSync(filepath);
  const raw = buf.toString("latin1");
  const textChunks: string[] = [];

  // Extract text between BT and ET markers (PDF text objects)
  const btEtRegex = /BT\s([\s\S]*?)ET/g;
  let match;
  while ((match = btEtRegex.exec(raw)) !== null) {
    const block = match[1];
    // Extract text from Tj and TJ operators
    const tjRegex = /\(([^)]*)\)\s*Tj/g;
    let tm;
    while ((tm = tjRegex.exec(block)) !== null) {
      textChunks.push(tm[1]);
    }
    // TJ arrays
    const tjArrayRegex = /\[((?:[^[\]]*|\[[^\]]*\])*)\]\s*TJ/g;
    while ((tm = tjArrayRegex.exec(block)) !== null) {
      const parts = tm[1].match(/\(([^)]*)\)/g);
      if (parts) {
        textChunks.push(parts.map(p => p.slice(1, -1)).join(""));
      }
    }
  }

  if (textChunks.length === 0) {
    return "[Could not extract text. Install poppler-utils (pdftotext) for better PDF support: sudo apt install poppler-utils]";
  }

  return textChunks.join("\n");
}

/** Get PDF metadata (page count, title, etc.) */
function getPdfInfo(filepath: string): { pages?: number; title?: string; author?: string } {
  try {
    const output = execSync(`pdfinfo ${JSON.stringify(filepath)}`, {
      encoding: "utf-8",
      timeout: 10000,
    });
    const pages = output.match(/Pages:\s+(\d+)/)?.[1];
    const title = output.match(/Title:\s+(.*)/)?.[1]?.trim();
    const author = output.match(/Author:\s+(.*)/)?.[1]?.trim();
    return { pages: pages ? parseInt(pages) : undefined, title: title || undefined, author: author || undefined };
  } catch {
    return {};
  }
}

export const pdfReadTool: Tool = {
  name: "pdf_read",
  description:
    "Extract text content from a PDF file. Uses pdftotext if available for best results, otherwise falls back to basic extraction. Returns the text content of the PDF.",
  parameters: [
    { name: "path", type: "string", description: "Path to the PDF file", required: true },
    { name: "pages", type: "string", description: "Page range (e.g., '1-5', '3'). Omit for all pages.", required: false },
    { name: "max_length", type: "number", description: "Max characters to return (default: 100000)", required: false, default: 100000 },
  ],
  async execute(args, context) {
    const filepath = isAbsolute(args.path as string)
      ? (args.path as string)
      : join(context.workingDir, args.path as string);

    if (!existsSync(filepath)) throw new Error(`PDF not found: ${filepath}`);

    if (extname(filepath).toLowerCase() !== ".pdf") {
      throw new Error("File does not appear to be a PDF");
    }

    const stat = readFileSync(filepath);
    if (stat.length > MAX_PDF_SIZE) {
      throw new Error(`PDF too large (${(stat.length / 1024 / 1024).toFixed(1)}MB). Max: 50MB`);
    }

    const pages = args.pages as string | undefined;
    const maxLength = (args.max_length as number) || 100000;

    const text = extractPdfText(filepath, pages);
    const info = getPdfInfo(filepath);

    return {
      path: filepath,
      size: stat.length,
      ...info,
      content: text.slice(0, maxLength),
      length: text.length,
      truncated: text.length > maxLength,
    };
  },
};
