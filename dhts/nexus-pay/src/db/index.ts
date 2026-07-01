import { Database } from "bun:sqlite";

export const db = new Database("nexus-pay.sqlite", { create: true });

export function initDb() {
  db.run(`
    CREATE TABLE IF NOT EXISTS accounts (
      id TEXT PRIMARY KEY,
      provider TEXT NOT NULL,
      name TEXT NOT NULL,
      api_key TEXT NOT NULL,
      billing_cycle_start DATE,
      monthly_budget_dollars REAL,
      current_usage_dollars REAL DEFAULT 0,
      current_usage_tokens INTEGER DEFAULT 0,
      payment_status TEXT CHECK( payment_status IN ('paid', 'unpaid', 'overdue') ) DEFAULT 'paid',
      last_checked DATETIME DEFAULT CURRENT_TIMESTAMP
    )
  `);

  db.run(`
    CREATE TABLE IF NOT EXISTS usage_history (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      account_id TEXT NOT NULL,
      timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
      tokens INTEGER NOT NULL,
      cost_dollars REAL NOT NULL,
      model TEXT,
      FOREIGN KEY(account_id) REFERENCES accounts(id)
    )
  `);
}
