import { createTeamChatServer } from "./server";

let close: (() => void) | undefined;

process.on("SIGTERM", () => { close?.(); process.exit(0); });
process.on("SIGINT", () => { close?.(); process.exit(0); });

createTeamChatServer().then(({ close: shutdown }) => {
  close = shutdown;
}).catch((err) => {
  console.error("[nexus-team-chat] failed to start:", err);
  process.exit(1);
});
