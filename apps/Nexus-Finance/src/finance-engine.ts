import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Expense {
  id: string;
  userId: string;
  amount: number;
  category: string;
  description: string;
  date: string;
  type: "expense" | "income";
  createdAt: string;
}

export interface Invoice {
  id: string;
  userId: string;
  invoiceNumber: string;
  clientName: string;
  amount: number;
  currency: string;
  status: "draft" | "sent" | "paid" | "overdue";
  dueDate: string;
  createdAt: string;
}

export class FinanceEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS expenses (
        id TEXT PRIMARY KEY,
        user_id TEXT,
        amount REAL,
        category TEXT,
        description TEXT,
        date TEXT,
        type TEXT DEFAULT 'expense',
        created_at TEXT
      );
      CREATE TABLE IF NOT EXISTS invoices (
        id TEXT PRIMARY KEY,
        user_id TEXT,
        invoice_number TEXT,
        client_name TEXT,
        amount REAL,
        currency TEXT,
        status TEXT,
        due_date TEXT,
        created_at TEXT
      );
    `);
    try {
      this.db.exec("ALTER TABLE expenses ADD COLUMN type TEXT DEFAULT 'expense'");
    } catch (_) {}
  }

  addExpense(e: {
    userId: string;
    amount: number;
    category?: string;
    description?: string;
    date?: string;
    type?: "expense" | "income";
  }): Expense {
    const x: Expense = {
      id: randomUUID(),
      userId: e.userId,
      amount: e.amount,
      category: e.category || "uncategorized",
      description: e.description || "",
      date: e.date || new Date().toISOString().slice(0, 10),
      type: e.type || "expense",
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO expenses (id, user_id, amount, category, description, date, type, created_at) VALUES (?,?,?,?,?,?,?,?)"
      )
      .run(x.id, x.userId, x.amount, x.category, x.description, x.date, x.type, x.createdAt);
    return x;
  }

  listExpenses(userId: string): Expense[] {
    return this.db
      .prepare("SELECT id, user_id as userId, amount, category, description, date, type, created_at as createdAt FROM expenses WHERE user_id = ? ORDER BY date DESC, created_at DESC")
      .all(userId) as Expense[];
  }

  getSummary(userId: string): {
    totalIncome: number;
    totalExpense: number;
    netBalance: number;
    byCategory: Record<string, number>;
  } {
    const rows = this.listExpenses(userId);
    const byCategory: Record<string, number> = {};
    let totalIncome = 0;
    let totalExpense = 0;

    for (const r of rows) {
      if (r.type === "income") {
        totalIncome += r.amount;
      } else {
        totalExpense += r.amount;
        byCategory[r.category] = (byCategory[r.category] || 0) + r.amount;
      }
    }

    return {
      totalIncome,
      totalExpense,
      netBalance: totalIncome - totalExpense,
      byCategory,
    };
  }

  createInvoice(inv: {
    userId: string;
    clientName: string;
    amount: number;
    currency?: string;
    dueDate?: string;
    invoiceNumber?: string;
  }): Invoice {
    const count = (this.db.prepare("SELECT COUNT(*) as c FROM invoices WHERE user_id = ?").get(inv.userId) as any)?.c || 0;
    const invNum = inv.invoiceNumber || `INV-${new Date().getFullYear()}-${String(count + 1).padStart(4, "0")}`;
    const x: Invoice = {
      id: randomUUID(),
      userId: inv.userId,
      invoiceNumber: invNum,
      clientName: inv.clientName,
      amount: inv.amount,
      currency: inv.currency || "USD",
      status: "draft",
      dueDate: inv.dueDate || new Date(Date.now() + 14 * 86400000).toISOString().slice(0, 10),
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO invoices (id, user_id, invoice_number, client_name, amount, currency, status, due_date, created_at) VALUES (?,?,?,?,?,?,?,?,?)"
      )
      .run(x.id, x.userId, x.invoiceNumber, x.clientName, x.amount, x.currency, x.status, x.dueDate, x.createdAt);
    return x;
  }

  listInvoices(userId: string): Invoice[] {
    return this.db
      .prepare(
        "SELECT id, user_id as userId, invoice_number as invoiceNumber, client_name as clientName, amount, currency, status, due_date as dueDate, created_at as createdAt FROM invoices WHERE user_id = ? ORDER BY created_at DESC"
      )
      .all(userId) as Invoice[];
  }

  updateInvoiceStatus(id: string, status: "draft" | "sent" | "paid" | "overdue"): boolean {
    const res = this.db.prepare("UPDATE invoices SET status = ? WHERE id = ?").run(status, id);
    return res.changes > 0;
  }
}
