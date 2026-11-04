import { createAuthServer } from "./server";

const { port, baseUrl } = createAuthServer();

console.log("Nexus Auth listening", {
  port,
  baseUrl,
  health: `${baseUrl}/health`,
  cookieDomain: process.env.NEXUS_AUTH_COOKIE_DOMAIN ?? "(not set — host-only cookies, cross-subdomain SSO disabled)",
});
