import { createAuthServer } from "./server";

const { port, baseUrl } = createAuthServer();

console.log("Nexus Auth listening", {
  port,
  baseUrl,
  health: `${baseUrl}/health`,
});
