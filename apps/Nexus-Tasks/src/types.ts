export interface Task {
  id: string;
  title: string;
  description: string;
  listId: string;
  assignee: string;
  priority: "low" | "medium" | "high" | "urgent";
  dueDate: string;
  completed: number;
  createdAt: string;
}

export interface TaskList {
  id: string;
  name: string;
  description: string;
  taskCount: number;
  createdAt: string;
}

export interface Assignee {
  id: string;
  name: string;
  email: string;
  role: string;
}
