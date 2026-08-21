import { lazy, Suspense } from "react";
import { isAdmin, type Me } from "../../api";
import type { TerminalSessionFactory } from "./TerminalView";

const TerminalView = lazy(() => import("./TerminalView"));

export type UserState =
  | { status: "loading" }
  | { status: "ready"; user: Me | null };

export default function TerminalAccess({
  userState,
  createSession,
}: {
  userState: UserState;
  createSession?: TerminalSessionFactory;
}) {
  if (userState.status === "loading") {
    return (
      <div className="flex h-full items-center justify-center p-8 text-sm text-zinc-500">
        Checking terminal access…
      </div>
    );
  }

  if (!userState.user || !isAdmin(userState.user)) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-2 p-8 text-center">
        <h1 className="text-lg font-semibold text-zinc-100">Terminal access required</h1>
        <p className="max-w-md text-sm text-zinc-500">
          Host terminal sessions are restricted to founder and administrator accounts.
        </p>
      </div>
    );
  }

  return (
    <Suspense
      fallback={
        <div className="flex h-full items-center justify-center p-8 text-sm text-zinc-500">
          Opening terminal…
        </div>
      }
    >
      <TerminalView user={userState.user} createSession={createSession} />
    </Suspense>
  );
}
