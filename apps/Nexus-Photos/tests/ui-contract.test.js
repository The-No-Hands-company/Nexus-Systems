import { expect, test } from "bun:test";

const source = (file) => Bun.file(new URL(`../frontend/src/${file}`, import.meta.url)).text();

test("Photos shell exposes a skip link, main target, and title", async () => {
  const app = await source("App.tsx");
  expect(app).toContain('href="#main-content"');
  expect(app).toContain('id="main-content"');
  expect(app).toContain("document.title");
});

test("Photos grid and header controls are keyboard and screen-reader ready", async () => {
  const grid = await source("components/PhotoGrid.tsx");
  const topBar = await source("components/TopBar.tsx");
  expect(grid).toContain("aria-pressed");
  expect(grid).toContain("onKeyDown");
  expect(grid).toContain("focus-visible");
  expect(topBar).toContain("<header");
  expect(topBar).toContain("aria-label");
  expect(topBar).toContain("focus-visible");
});
