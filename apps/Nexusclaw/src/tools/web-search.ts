// Web search tool — multi-backend search (Phase 3.2)
// Backends: SearXNG (self-hosted), Brave, Tavily, Jina, DuckDuckGo (fallback)

import type { Tool } from "../core/types.js";

// ─── Backend Interfaces ─────────────────────────────────────────────────

interface SearchResult {
  title: string;
  url: string;
  snippet: string;
  source: string;
}

interface SearchBackend {
  name: string;
  available(): boolean;
  search(query: string, limit: number): Promise<SearchResult[]>;
}

// ─── SearXNG Backend (self-hosted, no API key) ──────────────────────────

class SearXNGBackend implements SearchBackend {
  name = "searxng";
  private url: string;

  constructor() {
    this.url = process.env.SEARXNG_URL || "http://localhost:8080";
  }

  available(): boolean {
    return !!process.env.SEARXNG_URL;
  }

  async search(query: string, limit: number): Promise<SearchResult[]> {
    const params = new URLSearchParams({
      q: query,
      format: "json",
      categories: "general",
    });
    const res = await fetch(`${this.url}/search?${params}`, {
      headers: { Accept: "application/json" },
      signal: AbortSignal.timeout(15_000),
    });
    if (!res.ok) throw new Error(`SearXNG error: ${res.status}`);
    const data = (await res.json()) as { results: { title: string; url: string; content: string }[] };
    return data.results.slice(0, limit).map((r) => ({
      title: r.title,
      url: r.url,
      snippet: r.content || "",
      source: "searxng",
    }));
  }
}

// ─── Brave Search Backend ───────────────────────────────────────────────

class BraveBackend implements SearchBackend {
  name = "brave";
  private apiKey: string;

  constructor() {
    this.apiKey = process.env.BRAVE_SEARCH_API_KEY || "";
  }

  available(): boolean {
    return !!process.env.BRAVE_SEARCH_API_KEY;
  }

  async search(query: string, limit: number): Promise<SearchResult[]> {
    const params = new URLSearchParams({ q: query, count: String(limit) });
    const res = await fetch(`https://api.search.brave.com/res/v1/web/search?${params}`, {
      headers: {
        Accept: "application/json",
        "Accept-Encoding": "gzip",
        "X-Subscription-Token": this.apiKey,
      },
      signal: AbortSignal.timeout(15_000),
    });
    if (!res.ok) throw new Error(`Brave search error: ${res.status}`);
    const data = (await res.json()) as { web?: { results: { title: string; url: string; description: string }[] } };
    return (data.web?.results || []).slice(0, limit).map((r) => ({
      title: r.title,
      url: r.url,
      snippet: r.description || "",
      source: "brave",
    }));
  }
}

// ─── Tavily Backend ─────────────────────────────────────────────────────

class TavilyBackend implements SearchBackend {
  name = "tavily";
  private apiKey: string;

  constructor() {
    this.apiKey = process.env.TAVILY_API_KEY || "";
  }

  available(): boolean {
    return !!process.env.TAVILY_API_KEY;
  }

  async search(query: string, limit: number): Promise<SearchResult[]> {
    const res = await fetch("https://api.tavily.com/search", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        api_key: this.apiKey,
        query,
        max_results: limit,
        search_depth: "basic",
      }),
      signal: AbortSignal.timeout(15_000),
    });
    if (!res.ok) throw new Error(`Tavily error: ${res.status}`);
    const data = (await res.json()) as { results: { title: string; url: string; content: string }[] };
    return data.results.slice(0, limit).map((r) => ({
      title: r.title,
      url: r.url,
      snippet: r.content || "",
      source: "tavily",
    }));
  }
}

// ─── Jina Reader Backend (URL → text, search via reader API) ────────────

class JinaBackend implements SearchBackend {
  name = "jina";
  private apiKey: string;

  constructor() {
    this.apiKey = process.env.JINA_API_KEY || "";
  }

  available(): boolean {
    return !!process.env.JINA_API_KEY;
  }

  async search(query: string, limit: number): Promise<SearchResult[]> {
    const res = await fetch(`https://s.jina.ai/${encodeURIComponent(query)}`, {
      headers: {
        Accept: "application/json",
        Authorization: `Bearer ${this.apiKey}`,
        "X-Retain-Images": "none",
      },
      signal: AbortSignal.timeout(15_000),
    });
    if (!res.ok) throw new Error(`Jina search error: ${res.status}`);
    const data = (await res.json()) as { data: { title: string; url: string; description: string; content: string }[] };
    return (data.data || []).slice(0, limit).map((r) => ({
      title: r.title || "",
      url: r.url,
      snippet: r.description || r.content?.slice(0, 300) || "",
      source: "jina",
    }));
  }
}

// ─── DuckDuckGo Fallback (no API key needed, HTML scraping) ─────────────

class DuckDuckGoBackend implements SearchBackend {
  name = "duckduckgo";

  available(): boolean {
    return true; // Always available as fallback
  }

  async search(query: string, limit: number): Promise<SearchResult[]> {
    // Use DDG instant answer API (limited but free, no key)
    const params = new URLSearchParams({ q: query, format: "json", no_redirect: "1" });
    const res = await fetch(`https://api.duckduckgo.com/?${params}`, {
      headers: { "User-Agent": "AnyClaw/0.1 (web_search)" },
      signal: AbortSignal.timeout(10_000),
    });
    if (!res.ok) throw new Error(`DuckDuckGo error: ${res.status}`);
    const data = (await res.json()) as {
      AbstractText?: string;
      AbstractURL?: string;
      Heading?: string;
      RelatedTopics?: { Text: string; FirstURL: string }[];
    };

    const results: SearchResult[] = [];
    if (data.AbstractText && data.AbstractURL) {
      results.push({
        title: data.Heading || query,
        url: data.AbstractURL,
        snippet: data.AbstractText,
        source: "duckduckgo",
      });
    }
    for (const topic of data.RelatedTopics || []) {
      if (results.length >= limit) break;
      if (topic.Text && topic.FirstURL) {
        results.push({
          title: topic.Text.slice(0, 100),
          url: topic.FirstURL,
          snippet: topic.Text,
          source: "duckduckgo",
        });
      }
    }
    return results.slice(0, limit);
  }
}

// ─── Search Manager ─────────────────────────────────────────────────────

const backends: SearchBackend[] = [
  new SearXNGBackend(),
  new BraveBackend(),
  new TavilyBackend(),
  new JinaBackend(),
  new DuckDuckGoBackend(),
];

async function webSearch(query: string, opts: { backend?: string; limit?: number } = {}): Promise<SearchResult[]> {
  const limit = opts.limit || 10;

  // If user specified a backend, use it
  if (opts.backend) {
    const b = backends.find((x) => x.name === opts.backend);
    if (b && b.available()) return b.search(query, limit);
    throw new Error(`Search backend "${opts.backend}" not available. Configure its API key.`);
  }

  // Auto-select: try backends in priority order
  for (const b of backends) {
    if (!b.available()) continue;
    try {
      return await b.search(query, limit);
    } catch {
      continue; // Try next backend
    }
  }
  throw new Error("No search backends available. Set SEARXNG_URL, BRAVE_SEARCH_API_KEY, TAVILY_API_KEY, or JINA_API_KEY.");
}

// ─── Tool Definition ────────────────────────────────────────────────────

export const webSearchTool: Tool = {
  name: "web_search",
  description:
    "Search the web using multiple backends (SearXNG, Brave, Tavily, Jina, DuckDuckGo). Returns titles, URLs, and snippets. Use this to find information, documentation, APIs, or answers to questions.",
  parameters: [
    { name: "query", type: "string", description: "Search query", required: true },
    { name: "limit", type: "number", description: "Max results (default: 10)", required: false, default: 10 },
    { name: "backend", type: "string", description: "Force a specific backend (searxng, brave, tavily, jina, duckduckgo). Omit for auto-select.", required: false },
  ],
  async execute(args) {
    const results = await webSearch(args.query as string, {
      limit: (args.limit as number) || 10,
      backend: args.backend as string | undefined,
    });
    return {
      query: args.query,
      backend: results[0]?.source || "none",
      count: results.length,
      results: results.map((r) => ({
        title: r.title,
        url: r.url,
        snippet: r.snippet.slice(0, 300),
      })),
    };
  },
};

/** Get list of available search backends (for diagnostics) */
export function getAvailableBackends(): string[] {
  return backends.filter((b) => b.available()).map((b) => b.name);
}
