import { db, initDb } from "./db";
import { createProvider } from "./providers";

const VERSION = "2.0.0";

initDb();

function printHelp(): void {
  console.log(`⬡ Nexus-Pay v${VERSION} — AI Unified Proxy & Billing Manager

Usage:
  nexus-pay <command> [options]

Commands:
  add <provider> <name> <api_key> [budget]   Add an AI provider account
  list                                        List all accounts
  sync                                        Sync usage data from all providers
  use <name> [--default]                      Set account as active / default
  remove <name>                               Remove an account
  status                                      Show proxy health and account status
  health                                      Check proxy health (server mode)
  --help, -h                                  Show this help
  --version, -v                               Show version

Providers: openai, openrouter, anthropic

Examples:
  bun run src/cli.ts add openrouter "My Router" sk-or-xxx 50
  bun run src/cli.ts list
  bun run src/cli.ts sync
  bun run src/cli.ts use "My Router" --default
  bun run src/cli.ts status
  bun run src/cli.ts --version
`);
}

async function cmdAdd(): Promise<void> {
  const provider = process.argv[4];
  const name = process.argv[5];
  const apiKey = process.argv[6];
  const budget = parseFloat(process.argv[7] || "0");

  if (!provider || !name || !apiKey) {
    console.log("Usage: nexus-pay add <provider> <name> <api_key> [monthly_budget]");
    return;
  }

  const valid = ["openai", "openrouter", "anthropic"];
  if (!valid.includes(provider.toLowerCase())) {
    console.error(`Unknown provider: ${provider}. Valid: ${valid.join(", ")}`);
    return;
  }

  db.run(
    "INSERT INTO accounts (id, provider, name, api_key, monthly_budget_dollars) VALUES (?, ?, ?, ?, ?)",
    [crypto.randomUUID(), provider.toLowerCase(), name, apiKey, budget]
  );
  console.log(`✅ Added account "${name}" (${provider}) with $${budget} budget`);
  process.exit(0);
}

function cmdList(): void {
  const accounts = db.query("SELECT * FROM accounts ORDER BY created_at DESC").all() as any[];
  if (accounts.length === 0) {
    console.log("\nNo accounts configured. Use 'add' to add one.\n");
    return;
  }
  console.log("\n⬡ Nexus-Pay Accounts:\n");
  console.table(accounts.map(a => ({
    Name: a.name,
    Provider: a.provider,
    Budget: `$${a.monthly_budget_dollars}`,
    "Spent (tokens)": a.current_usage_tokens?.toLocaleString() || "0",
    Status: a.payment_status || "unknown",
    Active: a.is_default ? "⭐" : "",
  })));
  process.exit(0);
}

async function cmdSync(): Promise<void> {
  const accounts = db.query("SELECT * FROM accounts").all() as any[];
  if (accounts.length === 0) {
    console.log("No accounts to sync.");
    return;
  }
  console.log(`Syncing ${accounts.length} account(s)...`);
  for (const account of accounts) {
    try {
      const provider = createProvider(account.provider, { apiKey: account.api_key, accountId: account.id });
      const usage = await provider.getUsage();
      db.run(
        "UPDATE accounts SET current_usage_dollars = ?, current_usage_tokens = ?, last_checked = CURRENT_TIMESTAMP WHERE id = ?",
        [usage.dollarsSpent, usage.tokensUsed, account.id]
      );
      console.log(`  ✅ ${account.name}: $${usage.dollarsSpent.toFixed(4)} (${usage.tokensUsed?.toLocaleString() || 0} tokens)`);
    } catch (err: any) {
      console.error(`  ❌ ${account.name}: ${err.message}`);
    }
  }
  process.exit(0);
}

function cmdUse(): void {
  const name = process.argv[4];
  const isDefault = process.argv.includes("--default");

  if (!name) {
    console.log("Usage: nexus-pay use <name> [--default]");
    process.exit(1);
  }

  const account = db.query("SELECT * FROM accounts WHERE name = ?").get(name as any) as any;
  if (!account) {
    console.error(`Account not found: ${name}`);
    process.exit(1);
  }

  if (isDefault) {
    db.run("UPDATE accounts SET is_default = 0");
    db.run("UPDATE accounts SET is_default = 1 WHERE id = ?", [account.id]);
    console.log(`⭐ Set "${name}" as default account`);
  }
  console.log(`✅ Active account: ${name} (${account.provider})`);
  process.exit(0);
}

function cmdRemove(): void {
  const name = process.argv[4];
  if (!name) {
    console.log("Usage: nexus-pay remove <name>");
    process.exit(1);
  }
  const result = db.run("DELETE FROM accounts WHERE name = ?", [name]);
  if ((result as any).changes > 0) {
    console.log(`🗑️  Removed account: ${name}`);
  } else {
    console.error(`Account not found: ${name}`);
  }
  process.exit(0);
}

function cmdStatus(): void {
  const accounts = db.query("SELECT * FROM accounts ORDER BY is_default DESC").all() as any[];
  console.log("\n⬡ Nexus-Pay Status\n");
  console.log(`Version:     ${VERSION}`);
  console.log(`Accounts:    ${accounts.length}`);
  console.log(`Proxy Port:  ${process.env.PORT || 3999}`);
  console.log(`Database:    ${process.env.NEXUS_PAY_DB || "nexus-pay.sqlite"}`);

  if (accounts.length > 0) {
    console.log("\nAccounts:");
    for (const a of accounts) {
      const budget = a.monthly_budget_dollars || 0;
      const spent = a.current_usage_dollars || 0;
      const pct = budget > 0 ? Math.round((spent / budget) * 100) : 0;
      const alert = pct >= 80 ? " 🔴" : pct >= 50 ? " 🟡" : " 🟢";
      console.log(`  ${a.is_default ? "⭐" : "  "} ${a.name} (${a.provider}): $${spent.toFixed(2)} / $${budget} (${pct}%)${alert}`);
    }
  }
  console.log("");
  process.exit(0);
}

async function main(): Promise<void> {
  const command = process.argv[2];

  if (!command || command === "--help" || command === "-h") {
    printHelp();
    process.exit(0);
  }

  if (command === "--version" || command === "-v") {
    console.log(`nexus-pay v${VERSION}`);
    process.exit(0);
  }

  switch (command) {
    case "add":     await cmdAdd(); break;
    case "list":    cmdList(); break;
    case "sync":    await cmdSync(); break;
    case "use":     cmdUse(); break;
    case "remove":  cmdRemove(); break;
    case "status":  cmdStatus(); break;
    case "health":  console.log("✅ nexus-pay proxy is running"); process.exit(0); break;
    default:
      console.error(`Unknown command: ${command}`);
      printHelp();
      process.exit(1);
  }
}

main().catch(err => {
  console.error("Error:", err.message);
  process.exit(1);
});
