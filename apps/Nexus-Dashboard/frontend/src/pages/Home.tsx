import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { me, type Me } from "../api";
import Grid from "./Grid";

const AUTH_LOGIN_URL =
  import.meta.env.VITE_AUTH_LOGIN_URL ?? "https://auth.tnhc.dev/login";

/**
 * The root route decides what "home" means.
 *
 * Signed in, home is the app grid. Signed out, it is the way in — and this
 * host stays public precisely so a stranger can reach that. Gating it would
 * leave nowhere to request an account from.
 */
export default function Home() {
  const [user, setUser] = useState<Me | null | undefined>(undefined);

  useEffect(() => {
    let cancelled = false;
    void me().then((u) => { if (!cancelled) setUser(u); });
    return () => { cancelled = true; };
  }, []);

  if (user === undefined) {
    return <section className="mx-auto max-w-4xl p-8 text-zinc-500">Loading…</section>;
  }

  if (user) return <Grid />;

  return (
    <section className="mx-auto max-w-xl p-8">
      <h1 className="text-2xl font-semibold">Nexus</h1>
      <p className="mt-2 text-zinc-400">One account for every app in the ecosystem.</p>

      <div className="mt-6 flex flex-wrap gap-3">
        <a
          href={`${AUTH_LOGIN_URL}?redirect_uri=${encodeURIComponent(window.location.origin)}`}
          className="rounded bg-blue-600 px-4 py-2 font-medium"
        >
          Sign in
        </a>
        <Link to="/request" className="rounded border border-zinc-700 px-4 py-2">
          Request access
        </Link>
        <Link to="/claim" className="rounded border border-zinc-700 px-4 py-2">
          Claim your account
        </Link>
      </div>

      <p className="mt-6 text-sm text-zinc-500">
        Access is invite-only for now. Request an account and you will be given a claim code to
        save — there is no confirmation email.
      </p>
    </section>
  );
}
