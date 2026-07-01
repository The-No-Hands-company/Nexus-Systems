import { Hono } from "hono";
import { db } from "../db";
import { createProvider } from "../providers";

const VERSION = "2.0.0";

export const proxyApp = new Hono();

// ── Health endpoint ──────────────────────────────────────────────────────

proxyApp.get("/health", (c) => {
  const accounts = db.query("SELECT COUNT(*) as count FROM accounts").get() as any;
  const paidAccounts = db.query("SELECT COUNT(*) as count FROM accounts WHERE payment_status = 'paid'").get() as any;
  return c.json({
    service: "nexus-pay",
    version: VERSION,
    status: "ok",
    uptime: process.uptime(),
    accounts: { total: accounts?.count || 0, paid: paidAccounts?.count || 0 },
    timestamp: new Date().toISOString(),
  });
});

proxyApp.get("/", (c) => {
  return c.json({
    service: "nexus-pay",
    version: VERSION,
    endpoints: {
      health: "GET /health",
      chat: "POST /v1/chat/completions",
      accounts: "GET /v1/accounts",
      usage: "GET /v1/usage",
    },
  });
});

// ── Account listing ──────────────────────────────────────────────────────

proxyApp.get("/v1/accounts", (c) => {
  const accounts = db.query("SELECT id, provider, name, monthly_budget_dollars, current_usage_dollars, current_usage_tokens, payment_status, is_default FROM accounts ORDER BY is_default DESC").all();
  return c.json({ accounts });
});

// ── Usage summary ────────────────────────────────────────────────────────

proxyApp.get("/v1/usage", (c) => {
  const total = db.query("SELECT SUM(cost_dollars) as total_cost, SUM(tokens) as total_tokens FROM usage_history").get() as any;
  return c.json({
    totalCost: total?.total_cost || 0,
    totalTokens: total?.total_tokens || 0,
  });
});

// ── Chat completions proxy ───────────────────────────────────────────────
  const body = await c.req.json();
  const apiKey = c.req.header("Authorization")?.replace("Bearer ", "");

  // In Nexus-Pay, we might use a master key for the proxy, 
  // or pass-through if we just want to track.
  // For this standalone tool, we'll use the accounts configured in our DB.

  const accounts = db.query("SELECT * FROM accounts WHERE payment_status = 'paid' ORDER BY monthly_budget_dollars DESC").all() as any[];

  if (accounts.length === 0) {
    return c.json({ error: "No active paid accounts available" }, 503);
  }

  // Simple priority: first available account
  // In a real version, we'd check current_usage_dollars < monthly_budget_dollars
  for (const account of accounts) {
    try {
      console.log(`Attempting routing via ${account.provider} (${account.name})`);
      
      const response = await forwardToProvider(account, body);
      
      if (response.ok) {
        // Log usage in background
        const result = await response.clone().json();
        logUsage(account.id, result);
        
        return new Response(response.body, {
          status: response.status,
          headers: response.headers,
        });
      }
      
      console.warn(`Provider ${account.name} failed with status ${response.status}`);
    } catch (err) {
      console.error(`Failed to forward to ${account.name}:`, err);
    }
  }

  return c.json({ error: "All providers failed or exhausted" }, 502);
});

async function forwardToProvider(account: any, body: any) {
  let url = "";
  let headers: Record<string, string> = {
    "Content-Type": "application/json",
    Authorization: `Bearer ${account.api_key}`,
  };

  switch (account.provider) {
    case "openai":
      url = "https://api.openai.com/v1/chat/completions";
      break;
    case "anthropic":
      url = "https://api.anthropic.com/v1/messages";
      headers["x-api-key"] = account.api_key;
      headers["anthropic-version"] = "2023-06-01";
      delete headers["Authorization"];
      break;
    case "openrouter":
      url = "https://openrouter.ai/api/v1/chat/completions";
      headers["HTTP-Referer"] = "https://github.com/Nexus-Systems/nexus-pay";
      headers["X-Title"] = "Nexus-Pay";
      break;
    // Add more providers
    default:
      throw new Error(`Unsupported proxy provider: ${account.provider}`);
  }

  return fetch(url, {
    method: "POST",
    headers,
    body: JSON.stringify(body),
  });
}

async function logUsage(accountId: string, result: any) {
  const tokens = result.usage?.total_tokens || 0;
  // Cost calculation depends on model, we'll simplify for now
  const cost = 0; 

  db.run("INSERT INTO usage_history (account_id, tokens, cost_dollars, model) VALUES (?, ?, ?, ?)", 
    [accountId, tokens, cost, result.model]);
  
  db.run("UPDATE accounts SET current_usage_tokens = current_usage_tokens + ? WHERE id = ?", 
    [tokens, accountId]);
}
