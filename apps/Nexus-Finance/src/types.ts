export interface Expense {
  id: string;
  description: string;
  amount: number;
  category: string;
  type?: "expense" | "income";
  date: string;
  createdAt: string;
}

export interface Invoice {
  id: string;
  client: string;
  amount: number;
  status: "draft" | "sent" | "paid" | "overdue";
  dueDate: string;
  createdAt: string;
}

export interface FinanceSummary {
  totalIncome: number;
  totalExpenses: number;
  netBalance: number;
  categories: Record<string, number>;
}
