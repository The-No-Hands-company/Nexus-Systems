import { Badge, type BadgeVariant } from "../../../../../packages/nexus-design/src/components/ui/badge";
import type { Status, StatusCategory } from "../types";

const VARIANT: Record<StatusCategory, BadgeVariant> = {
  backlog:   "default",
  unstarted: "default",
  started:   "info",
  completed: "success",
  canceled:  "default",
};

export function StatusBadge({ status }: { status: Status }) {
  return <Badge variant={VARIANT[status.category]}>{status.name}</Badge>;
}
