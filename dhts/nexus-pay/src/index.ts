import { serve } from "bun";
import { initDb } from "./db";
import { proxyApp } from "./proxy/server";

// Initialize the database
initDb();

console.log("⬡ NEXUS-PAY — AI Unified Proxy & Billing Manager");

// Start the proxy server
const port = process.env.PORT || 3999;
serve({
  fetch: proxyApp.fetch,
  port: Number(port),
});

console.log(`Proxy server running on http://localhost:${port}`);
console.log("Ready to route and track your AI bills.");
