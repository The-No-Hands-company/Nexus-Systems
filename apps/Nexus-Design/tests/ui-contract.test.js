import { expect, test } from "bun:test";

const source = (file) => Bun.file(new URL(`../frontend/src/${file}`, import.meta.url)).text();

test("Design shell exposes a skip link, main target, and title", async () => {
  const app = await source("App.tsx");
  expect(app).toContain('href="#main-content"');
  expect(app).toContain('id="main-content"');
  expect(app).toContain("document.title");
});

test("Design controls expose names, focus states, and token accents", async () => {
  const toolbar = await source("components/Toolbar.tsx");
  const topBar = await source("components/TopBar.tsx");
  expect(toolbar).toContain("aria-label");
  expect(toolbar).toContain("focus-visible");
  expect(toolbar).not.toContain("bg-blue-600");
  expect(topBar).toContain("<header");
  expect(topBar).toContain("focus-visible");
});
