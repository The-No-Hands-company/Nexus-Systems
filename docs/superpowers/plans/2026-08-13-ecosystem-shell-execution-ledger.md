# SDD ledger — plan: docs/superpowers/plans/2026-08-13-ecosystem-shell-and-design-system.md

Branch: main (standing authority from the founder; session configured to work in place)
Started: 2026-08-13T07:44:19+02:00

Pre-flight: Tasks 9/10 duplicate `isEmbedded` verbatim — reviewers flag this.
  Ruling: duplication stands. apps/Nexus is a separate git repository and cannot
  import from the parent repo's packages/. Plan rationale corrected from a weak
  argument (release coupling) to the real one (repo boundary). No other conflicts.
Task 1: complete (commits 73c4e7b9..59f11043, review clean)
  Reviewer note carried to Task 2: metadata skip only applies at top level;
  Task 2 exercises that path with the real token file.
Task 2: review found 2 Important, both plan-mandated (the plan specified the code).
  Ruling (delegated technical authority): both are real and load-bearing — fix now.
  (a) numeric tokens emitted unitless: `--nexus-space-4: 16` is invalid CSS.
  (b) Tailwind v4 namespaces wrong: ~34/57 tokens produce no utility, so p-4 and
      text-sm silently keep Tailwind defaults — the pipeline's whole purpose lost.
  Plan Global Constraints amended with unit rules and the v4 namespace map.
Task 2: fix round 1/5 (2 addressed, 0 open; commits 9ff2d866..ef1ddd91)
Task 2: complete (commits 59f11043..ef1ddd91, review clean)
  Deferred minor: the two drift tests use a duplicated spec-mirror helper, so a
  shared bug could pass both; the concrete literal assertions backstop it.
Task 3: implementer reported BLOCKED — "Tailwind v4 does not process @theme from
  imported files". Controller verified: that diagnosis is WRONG. Adding a
  bg-bg-canvas usage made --color-bg-canvas:#090d0d appear in the build. The
  import works; v4 tree-shakes unused theme vars. The plan's verification (grep
  for the accent colour, which nothing uses yet) was the defect. Corrected to
  assert --spacing-4:16px / --text-sm:14px, tokens the app already uses.
Task 3: NOTE — controller error. My `git add -A` in 35380bce (a plan-doc commit)
  swept up index.css and package.json, so Task 3's deliverable lives in a commit
  whose message says docs. Content is correct and the tree is clean; history is
  misleading. Not rewritten — already pushed. Plan now forbids `git add -A`.
  Baseline correction: Dashboard frontend suite is 36 tests, not the 21 the plan
  claimed; 21 was stale when the plan was written.
Task 3: fix round 1/5 (1 addressed, 0 open — tsbuildinfo untracked; commit c75d43b0)
Task 3: complete (commits ef1ddd91..c75d43b0, review clean after fix)
  Both Task 3 findings were controller staging errors, not implementer errors.
Task 4: review found 2 Important, both traceable to my brief.
  (a) brief's Interfaces promised a `navigation` landmark from Shell; wrong —
      <aside> is `complementary`, and Task 5's Launcher supplies <nav> inside it.
      Plan corrected; fix gives the aside an accessible name so it is assertable.
  (b) the sidebar test asserts presence but not placement — content rendered into
      main would still pass. Real weakness; fix scopes the assertion.
Task 4: fix round 1/5 (2 addressed, 0 open; commits dfab8982..5fce9ffa)
Task 4: complete (commits c75d43b0..5fce9ffa, review clean after fix)
Task 5: review raised 2 Critical. Both adjudicated as reviewer error, with evidence.
  (a) "fabricated citation" — the implementer cited my dispatch instruction ("do not
      add a second aria-label=Applications to your <nav>"), which the reviewer never
      received because I passed only the brief. Implementer obeyed and disclosed;
      that is correct behaviour, not a trust problem. Ruling: no defect.
  (b) "test count discrepancy" — reviewer read my stated baseline (40) as the
      expected final. Verified directly: 46 passing, 8 files. Implementer correct.
      The reviewer itself marked this "cannot verify from diff", so resolving it was
      the controller's job. Ruling: no defect.
  PROCESS FIX: every later review dispatch must include the dispatch context given
  to the implementer, not just the brief. Two false Criticals came from that gap.
Task 5: complete (commits 5fce9ffa..15dfb204, 2 findings adjudicated as reviewer error)
Task 6: complete (commits 15dfb204..32c789e3, review clean)
Task 6: minor (deferred): unknown-app test asserts no iframe titled "Draw" rather
  than no iframe at all; container.querySelector("iframe") would be tighter.
  (Process fix worked — including dispatch context in the review prompt produced
  no false findings this round.)
Task 7: review found 1 Important (plan-mandated): app list starts [] with no
  loading state, so a valid appId shows "App not found." until listApps resolves,
  and forever if it rejects. Real and user-visible. Ruling: fix now. Plan amended.
Task 7: minor (deferred): /claim test proves banner absence but not that Claim's
  own content rendered; no false-pass risk today since Claim is synchronous.
Task 7: fix round 1/5 (1 addressed, 0 open; commits 9271341a..df644c37)
Task 7: complete (commits 32c789e3..df644c37, review clean after fix)
Task 7: open follow-up (not this plan): whether /, /account, /admin gain the shell.
Task 8: complete for chat (commit 1c9a683d) — one CSP header, correct
  frame-ancestors, X-Frame-Options gone. Verified independently.
Task 8: INCOMPLETE for draw. draw.tnhc.dev is a Nexus-Hosting static site
  (x-served-by: nexus-proxy/rust), not the Bun server on :3075 the implementer
  hardened. Every Nexus-Hosting site still has zero framing protection.
  Correction dispatched as Task 8b; brief at task-8b-brief.md. BASE 1c9a683d.
Task 9: brief CORRECTED before dispatch. Original Step 6 verified via
  `deploy.sh bg`, which only restarts Draw's :3075 API — draw.tnhc.dev is a
  Hosting static site, so the embed change would have passed every local check
  and never reached users. New brief ships via Hosting's `nh` CLI to site 5,
  and asserts the deployed index.html references the newly built asset hashes.
  Also added a component test (TopBar gated) — the isEmbedded unit tests only
  prove the parser, not that App obeys it.
Task 9: deploy credential minted — api_tokens id 6 `sdd-shell-task9`
  (read,write,deploy), plaintext at ~/.config/nexus-cli-nodejs/.deploy-token
  0600, never in the repo. Revoke when the plan lands.
Task 9: note — 3 older active deploy tokens exist (ids 1, 4, 5) from earlier
  sessions. Token sprawl; worth revoking, not doing it mid-plan.
Task 10/11: checked for the same blind spot — clear. Chat's Caddy serves
  apps/Nexus/packages/nexus-web/dist from disk, so a rebuild suffices; Cloud is
  a deploy.sh-managed process.
Task 8b: complete (submodule 8c59ecd+a3e6623, pointer 7131ffe0). draw.tnhc.dev
  now serves frame-ancestors 'self' https://app.tnhc.dev from the real origin
  (Rust hosting proxy), configurable via PROXY_FRAME_ANCESTORS default 'self'.
  Chat non-regressed. Six hosts unchanged.
Task 8b: REGRESSION introduced and fixed by me. 8b also dropped the ports block
  from Hosting's redis AND minio, reasoning the root infra stack owns
  9000/9001. True for redis (never published); false for minio —
  storage.tnhc.dev is a tunnel ingress onto that host port and presigned upload
  URLs are signed against that hostname. storage.tnhc.dev went 502 and every
  site deploy would have failed at the PUT step. Root cause was the port
  NUMBER: the running container was always 9010->9000 while compose said
  9000:9000 and could never bind. Restored at 9010.
  Fix: submodule 4986899, pointer 4e403d94. Verified 502 -> 403.
Task 8b: lesson for later dispatches — an agent that resolves an unrelated
  obstacle mid-task must have that resolution verified independently. The
  framing work itself was correct; the collateral change was the damage.
Task 9: ship path PROVEN end to end before dispatch (presign -> PUT 200 ->
  register 201 -> deploy active -> live content matched stamp), site 3.
  Brief updated: REST pipeline is verified, `nh` CLI preferred but unproven.
Task 9: minor (deferred, 8b): `cargo test` panics in debug on a pre-existing
  clap bug (`low_resource: bool` lacks #[arg(long)]); release builds unaffected.
Task 9: minor (deferred, 8b): hosting proxy sets no X-Content-Type-Options on
  user-uploaded content. Deliberately out of scope; worth its own decision.
Task 9: complete (commit 68a04982, review clean, zero findings). Tests
  107 -> 114. Shipped live and independently verified: draw.tnhc.dev index
  references index-DST8bpFL.js matching local dist, embed logic in the live
  bundle. Reviewer reverted the gate to confirm the test actually fails.
Task 9: minor (deferred): Hosting `nh` CLI is unusable on this node — `npm
  install` hits an unresolved pnpm `catalog:` ref, and `nh login` crashes
  probing reachability with a stale config nodeUrl instead of the --node flag.
  Deploys work via REST. Worth fixing; the CLI is the documented user path.
Task 10: brief REWRITTEN before dispatch — the plan's premise was wrong.
  It said "Chat has both a server rail and a header; hide those." Chat has NO
  header (MainLayout is 3 columns: ServerList | ChannelList | ChatView), and
  the server rail is Chat's own navigation — hiding it removes server switching
  and the only discoverable route to Chat settings. Chat has no duplicate
  chrome. DECISION (mine, per the standing working agreement): the one genuine
  embed defect is the "Enable notifications" banner — chat.tnhc.dev framed by
  app.tnhc.dev is cross-origin, where browsers refuse
  Notification.requestPermission(), so it offers a button that cannot work and
  whose failure the handler already swallows. Task 10 suppresses that and
  nothing else. USER-VISIBLE, so flag it in the final report.
Task 10: nexus-web has NO test tooling at all (no vitest/config/tests). Adding
  vitest only — the design puts the decision in a pure shouldShowNotifBanner()
  so no jsdom or testing-library is needed. MainLayout would have required
  mocking gateway+store+voice+push+router to assert less.
Task 10: apps/Nexus is a separate repo (submodule), clean on main — two commits.
Task 10: complete (submodule c186a03, pointer d370401f; review clean, zero
  findings). 10 tests pass; nexus-web has a test runner for the first time.
  Live verified by me: chat.tnhc.dev serves index-BdABnHTR.js matching dist.
  Two brief inaccuracies the implementer caught and reported instead of
  papering over: the original chain checked Notification.permission ===
  "default" (needed a 5th field) and had no session check. Reviewer traced the
  algebra and confirmed behaviour-preserving except the intended embed gate.
Task 11: brief REPLACED before dispatch — the original was destructive.
  It deleted apps/Nexus-Cloud/public/status.html and returned a JSON pointer.
  Measured: cloud.tnhc.dev serves a WORKING 99KB control-plane console (200,
  99597 bytes); the shell has NO Cloud views (only Shell/Launcher/AppFrame/
  apps.ts); and the shell's launcher already lists nexus-cloud, so
  app.tnhc.dev/a/nexus-cloud frames the console and works TODAY. The design's
  build order paired retirement with "rebuilt as shell-native views" — the plan
  kept the deletion and dropped the rebuild. Executing it would have removed a
  working console and left nothing.
  DECISION (mine): keep status.html; make Cloud a proper shell citizen instead.
  Also closes a real hole — cloud.tnhc.dev sends NO framing headers, so the
  ecosystem's control plane is framable by any site on the internet. It is the
  last exposed surface; chat and draw were fixed earlier in this plan.
  DEFERRED as its own project: rebuild Cloud's console as shell-native views
  and let Cloud shed its frontend. USER-VISIBLE decision — flag in final report.
Task 11: also drops the plan's `git add -A apps/Nexus-Cloud`, which contradicts
  the plan's own Global Constraints.
Task 11: complete (submodule 240917a, pointer fddb0778). Implementer was cut
  off by a session limit after writing the code and its 5 tests but before
  running the suite, deploying or committing; I finished those myself.
  Tests 83 -> 88 pass, same 2 pre-existing nexus-certificate failures.
  Live verified: cloud / 200 100094 bytes (still the full HTML console, not a
  JSON stub); CSP frame-ancestors 'self' https://app.tnhc.dev; ?embed=1 yields
  <html lang="en" class="embedded">, standalone does not; API routes unchanged.
  Six hosts: app 200, auth 404, chat 302, draw 302, cloud 200, hosting 200.
Task 11: my brief was wrong that Nexus-Cloud is not a submodule — it is one,
  so the task took two commits. Also note apps/Nexus-Cloud/.gitignore-ancestry
  ignores public/, though status.html is tracked so the edit committed fine.
Task 11: deploy.sh bg is idempotent and did NOT pick up the new Cloud code —
  had to stop PID 1199928's predecessor (3869737, read from the pid file, never
  pkill) and rerun. Same trap that caught the Task 8b implementer. Any future
  task changing a running service must verify the change is actually live, not
  just that deploy.sh reported success.

## Final whole-branch review (opus) + fix round
Verdict was "fix first": 2 Critical, 3 Important, 4 Minor. All independently
confirmed by me before dispatch. Highlights:
  C1 The SHELL ITSELF was framable by anyone — app.tnhc.dev sent no framing
     headers. The plan closed this hole on Draw and then built a higher-value
     target (authenticated launcher/account/admin) and left it open.
     Fixed: frame-ancestors 'self' (permits nobody; nothing frames the shell).
  C2 Draw LOST FUNCTION when embedded. TopBar was the only caller of
     downloadPNG/downloadSVG and the only mount of HelpOverlay — hiding it left
     embedded Draw with no export and no help, a dual-mode violation.
     Fixed: compact TopBar when embedded (branding suppressed, export+help
     kept). The Google Docs shape: product bar on top, document toolbar beneath.
  I3 Task 8b's only test was committed RED — a clap debug-assert panicked before
     any test ran, so the frame-ancestors default had zero working coverage.
  I4 Chat's Rust middleware still sent X-Frame-Options: SAMEORIGIN alongside the
     new CSP, plus frame-ancestors * — the exact contradiction the plan claimed
     to have removed. Only the Caddy document path had been fixed.
  I5 DEFERRED, correctly: no app links nexus-tokens.css, so the design language
     reaches only the shell. Confirmed inert to add (tokens are --nexus-*
     prefixed; apps use bare names) — this is the NEXT PROJECT (apps adopting
     the tokens), not a patch. The plan's self-review wrongly claimed full
     spec coverage.
All 6 dispatched fixes closed; re-reviewer reverted each and confirmed the tests
fail without them. No new defects.
Suites green: nexus-design 24, Dashboard backend 22, Dashboard frontend 56,
Draw 115, Cloud 88 (+2 pre-existing cert failures), nexus-proxy 1.
Live: app frame-ancestors 'self'; chat API no X-Frame-Options; six hosts OK.
