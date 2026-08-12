const TOOL_ID = "nexus-dashboard";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function intervalMs(): number {
  return Math.max(5000, Number(process.env.NEXUS_DASHBOARD_HEARTBEAT_INTERVAL_MS || "30000"));
}

function enabled(): boolean {
  return (process.env.NEXUS_DASHBOARD_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

/**
 * Tell Cloud this app exists and is alive.
 *
 * Not optional bookkeeping: Cloud marks a tool `offline` when it stops
 * hearing from it, and Guardian refuses to expose an offline tool — so a
 * dashboard that never heartbeats can register an address for app.<domain>
 * and have it sit at `requested` forever, never routing. That is exactly what
 * happened when this file did not exist.
 *
 * Best-effort throughout: Cloud being down must not stop the dashboard from
 * serving, since the account pages work without it.
 */
export function startCloudHeartbeat(upstreamUrl: string): () => void {
  if (!enabled()) return () => {};

  const announce = async () => {
    try {
      await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
        method: "POST",
        headers: cloudHeaders(),
        body: JSON.stringify({
          id: TOOL_ID,
          name: "Nexus Dashboard",
          description: "Ecosystem front door — sign in, your apps, your account",
          upstreamUrl,
          // Deliberately NOT PUBLIC_URL: that name is generic enough that a
          // sibling service's env file set it to chat.tnhc.dev and this app
          // announced itself under Chat's hostname. Own the variable name.
          publicUrl:
            process.env.NEXUS_DASHBOARD_PUBLIC_URL ||
            `https://app.${process.env.DOMAIN || "tnhc.dev"}`,
          capabilities: ["dashboard", "account", "admin"],
          health: "healthy",
        }),
      });
      await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent(TOOL_ID)}/heartbeat`, {
        method: "POST",
        headers: cloudHeaders(),
        body: JSON.stringify({ health: "healthy", upstreamUrl }),
      });
    } catch {
      // Unreachable Cloud is survivable; the next tick tries again.
    }
  };

  void announce();
  const timer = setInterval(() => void announce(), intervalMs());
  return () => clearInterval(timer);
}
