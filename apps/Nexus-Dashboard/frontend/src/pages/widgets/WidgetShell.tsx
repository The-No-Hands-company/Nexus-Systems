import type { ReactNode } from "react";
import { EmptyState } from "../../../../../../packages/nexus-design/src/components/ui/empty-state";
import { Skeleton } from "../../../../../../packages/nexus-design/src/components/ui/skeleton";

export type WidgetState<T> =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; data: T };

/**
 * The four states, in one place, so no widget improvises its own.
 *
 * The distinction that matters: an empty array renders the empty copy, a failed
 * request renders the error. Collapsing those two is how a broken API becomes
 * an apparently quiet day.
 */
export default function WidgetShell<T>({
  title, state, empty, children,
}: {
  title: string;
  state: WidgetState<T>;
  empty: string;
  children: (data: T) => ReactNode;
}) {
  return (
    <section className="rounded-lg border border-zinc-600 bg-zinc-800 p-[var(--nexus-widget-padding)]">
      <h2 className="mb-3 font-mono text-xs uppercase tracking-[0.18em] text-zinc-500">{title}</h2>
      {state.status === "loading" ? <Skeleton lines={3} /> : null}
      {state.status === "error" ? (
        <p role="alert" className="py-4 text-sm text-state-danger">{state.message}</p>
      ) : null}
      {state.status === "ready"
        ? (children(state.data) ?? <EmptyState title={empty} />)
        : null}
    </section>
  );
}
