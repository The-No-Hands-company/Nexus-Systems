import { describe, expect, it } from "bun:test";
import { FinanceEngine } from "../src/finance-engine";

describe("Nexus Finance Ledger Engine", () => {
  it("tracks expenses, income and computes net balances", () => {
    const engine = new FinanceEngine(":memory:");
    engine.addExpense({
      userId: "usr-1",
      description: "Cloudflare Pro",
      amount: 25.0,
      category: "Infrastructure",
      type: "expense",
      date: "2026-11-10",
    });

    engine.addExpense({
      userId: "usr-1",
      description: "Sponsorship Grant",
      amount: 500.0,
      category: "Revenue",
      type: "income",
      date: "2026-11-10",
    });

    const summary = engine.getSummary("usr-1");
    expect(summary.totalExpense).toBe(25.0);
    expect(summary.totalIncome).toBe(500.0);
    expect(summary.netBalance).toBe(475.0);
  });
});
