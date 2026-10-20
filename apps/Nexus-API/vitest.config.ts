import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    // Runs before every test file, so the suite is runnable from a clean
    // shell. Without it three files fail to import and vitest still prints a
    // green count for the rest. See tests/setup.ts.
    setupFiles: ["./tests/setup.ts"],
    // Tests that spin up Bun.serve import bun:test and can only run under
    // `bun test`. Exclude them from vitest so check.sh stays green.
    exclude: [
      "**/node_modules/**",
      "tests/public-api.test.ts",
      "tests/server.test.ts",
      "tests/delegation-boundary.test.ts",
      "tests/calendar-boundary.test.ts",
    ],
  },
});
