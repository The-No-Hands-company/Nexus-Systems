# Creative UI Green Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task by task with verification after each task.

**Goal:** Bring the active Nexus-Design, Nexus-Graphic, Nexus-Photos, and Nexus-Video frontends to the same accessibility, responsive, interaction, and visual quality bar as the completed Green-pass apps.

**Architecture:** Keep each frontend self-contained and preserve its existing Zustand stores and canvas/video behavior. Improve the existing shell, navigation, controls, and content semantics in place. Add small source-contract tests where the apps currently have no browser test harness; these tests prevent the accessibility regressions that the pass is correcting.

**Tech Stack:** React 19, TypeScript strict mode, Vite, Tailwind CSS, Bun tests, Zustand, Lucide icons where already used.

## Global Constraints

- Touch only the four listed frontends and their focused tests. Preserve unrelated working-tree changes.
- Do not change backend/API contracts or add new runtime dependencies.
- Every interactive element must have a keyboard path, an accessible name, and a visible `:focus-visible` state.
- Use the existing accent token (`accent`) instead of introducing new blue/red shell colors.
- Keep canvas and media pointer behavior intact while adding keyboard and screen-reader affordances around it.

---

### Task 1: Add regression tests for shell and control contracts

**Files:**
- Create: `apps/Nexus-Design/tests/ui-contract.test.js`
- Create: `apps/Nexus-Graphic/tests/ui-contract.test.js`
- Create: `apps/Nexus-Photos/tests/ui-contract.test.js`
- Create: `apps/Nexus-Video/tests/ui-contract.test.js`

**Test first:** Each test reads the app's source files and fails until the expected contract exists: skip link and `main` target, document title, semantic header/navigation, named controls, focus-visible classes, and no nested interactive controls.

**Verification:** Run each app's focused contract test and confirm the new assertions fail before implementation.

### Task 2: Green-pass Nexus-Design and Nexus-Graphic shells

**Files:**
- Modify: `apps/Nexus-Design/frontend/src/App.tsx`
- Modify: `apps/Nexus-Design/frontend/src/components/TopBar.tsx`
- Modify: `apps/Nexus-Design/frontend/src/components/Toolbar.tsx`
- Modify: `apps/Nexus-Design/frontend/src/components/LayerPanel.tsx`
- Modify: `apps/Nexus-Design/frontend/src/components/PropertiesPanel.tsx`
- Modify: `apps/Nexus-Graphic/frontend/src/App.tsx`
- Modify: `apps/Nexus-Graphic/frontend/src/components/TopBar.tsx`
- Modify: `apps/Nexus-Graphic/frontend/src/components/Toolbar.tsx`
- Modify: `apps/Nexus-Graphic/frontend/src/components/LayerPanel.tsx`
- Modify: `apps/Nexus-Graphic/frontend/src/components/PropertiesPanel.tsx`

Add skip links and stable `main` landmarks, update titles from the active project, make the top bar a banner, label tool and sidebar regions, give toolbar/layer/property controls names and focus-visible rings, raise touch targets to 44px, make status text reactive through store selectors, and replace hardcoded blue shell accents with the design accent token. Keep layer selection keyboard reachable without nesting buttons inside clickable rows.

**Verification:** Run `bun run check`, `bun run build`, `bun test`, and the focused contract test in each frontend.

### Task 3: Green-pass Nexus-Photos

**Files:**
- Modify: `apps/Nexus-Photos/frontend/src/App.tsx`
- Modify: `apps/Nexus-Photos/frontend/src/components/TopBar.tsx`
- Modify: `apps/Nexus-Photos/frontend/src/components/Sidebar.tsx`
- Modify: `apps/Nexus-Photos/frontend/src/components/PhotoGrid.tsx`
- Modify: `apps/Nexus-Photos/frontend/src/components/PhotoViewer.tsx`
- Modify: `apps/Nexus-Photos/frontend/src/components/PhotoEditor.tsx`

Add a skip link and main landmark, title updates, responsive sidebar/info behavior, semantic navigation, labelled search and view controls, keyboard-accessible photo tiles with selected state, visible focus rings, and labelled editor/viewer controls. Preserve the existing double-click and keyboard shortcuts while making Enter/Space usable for the same primary actions.

**Verification:** Run `bun run check`, `bun run build`, `bun test`, and the focused contract test.

### Task 4: Green-pass Nexus-Video

**Files:**
- Modify: `apps/Nexus-Video/frontend/src/App.tsx`
- Modify: `apps/Nexus-Video/frontend/src/components/TopBar.tsx`
- Modify: `apps/Nexus-Video/frontend/src/components/Sidebar.tsx`
- Modify: `apps/Nexus-Video/frontend/src/components/VideoGrid.tsx`
- Modify: `apps/Nexus-Video/frontend/src/components/VideoPlayer.tsx`
- Create: `apps/Nexus-Video/frontend/src/lucide-icons.d.ts`
- Modify: `apps/Nexus-Video/frontend/src/components/ChannelPage.tsx`

Add a skip link and main landmark, title updates, responsive sidebar and playlist regions, labelled header controls, focus-visible states, semantic channel/navigation headings, named video cards, and accessible media controls. Preserve pointer and keyboard playback shortcuts; expose seek/volume/playback controls to assistive technology without changing the store contract.

Import only the icons used by the video surface and provide local declarations for those stable package paths so Vite does not transform the entire icon barrel during production builds.

**Verification:** Run `bun run check`, `bun run build`, `bun test`, and the focused contract test.

### Task 5: Cross-app audit and delivery

Run all four apps' full gates from clean app-local paths, run `git diff --check`, inspect the final diff for nested interactive elements and hardcoded shell colors, and verify only the planned files changed. Commit each app's changes with a focused message, update the parent pointer only where applicable, then push `main`.

**Expected evidence:** Four passing type/build/test gates, four passing contract tests, clean diff check, and a final file-scope audit with unrelated work preserved.
