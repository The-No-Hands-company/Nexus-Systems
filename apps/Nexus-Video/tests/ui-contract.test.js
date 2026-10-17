import { expect, test } from "bun:test";

const source = (file) => Bun.file(new URL(`../frontend/src/${file}`, import.meta.url)).text();

test("Video shell exposes a skip link, main target, and title", async () => {
  const app = await source("App.tsx");
  expect(app).toContain('href="#main-content"');
  expect(app).toContain('id="main-content"');
  expect(app).toContain("document.title");
});

test("Video controls expose names, focus states, and media labels", async () => {
  const topBar = await source("components/TopBar.tsx");
  const grid = await source("components/VideoGrid.tsx");
  const player = await source("components/VideoPlayer.tsx");
  expect(topBar).toContain("<header");
  expect(topBar).toContain("aria-label");
  expect(topBar).toContain("focus-visible");
  expect(grid).toContain("focus-visible");
  expect(player).toContain("aria-label");
  expect(player).toContain("focus-visible");
});
