# Nexus Systems Ecosystem Graph (MVP)

This document is the evolving ecosystem outline for Nexus Systems.

The No Hands Company Nexus Systems ecosystem is a suite of privacy-first, self-hosted, open-source products that work as standalone tools and as an integrated platform.

The architecture intent is:

- every product can run independently
- every product can integrate through the ecosystem contract
- Nexus Cloud acts as the control-plane hub for cross-system topology and discovery

Core value addendum: universal accessibility

- The ecosystem must remain accessible across browsers, operating systems, and device classes.
- Browser access is mandatory because it is the most universal runtime available to users globally.
- Every major capability should be reachable from web, and where useful also from mobile and desktop surfaces.
- Cross-browser compatibility and responsive behavior are non-negotiable quality requirements for production readiness.
- Local development must preserve this principle by testing interfaces and workflows in multiple environments, not just one primary machine/browser.

Metadata pattern used by graph extraction

Use this exact lightweight pattern in docs to make relationships machine-readable:

system: <system-name>
owns: <item-1>, <item-2>
depends_on: <system-or-item-1>, <system-or-item-2>
exposes: <api-or-surface-1>, <api-or-surface-2>
integrates_with: <system-1>, <system-2>

Keep values comma-separated. One metadata block describes one system.

Ecosystem topology blocks

system: Nexus Cloud
owns: ecosystem topology model, service discovery registry, cloud control-plane APIs
depends_on: Nexus Network, Nexus Vault
exposes: /.well-known/nexus-cloud, /api/cloud/discovery, /api/cloud/register, /api/cloud/client
integrates_with: Nexus, Nexus-AI, Nexus-Computer, Nexus-Deploy, Nexus-Hosting, Nexus-Network, Nexus-Porter, Nexus-Vault, Nexusclaw, Nit, Phantom

system: Nexus
owns: privacy-first communication platform, gateway, voice subsystem
depends_on: Nexus Cloud, Nexus Hosting
exposes: api service, gateway service, voice service
integrates_with: Nexus Cloud, Nexus-AI, Nexus-Computer, Nexus Network

system: Nexus-AI
owns: openai-compatible inference gateway, model routing, autonomous coding workflows
depends_on: Nexus Cloud
exposes: /v1/models, /v1/chat/completions
integrates_with: Nexus Cloud, Nexusclaw, Nexus-Computer

system: Nexus-Computer
owns: personal cloud computer runtime, agent shell/file orchestration, local-first productivity UI
depends_on: Nexus-AI, Nexus Cloud
exposes: agent backend api, personal cloud frontend
integrates_with: Nexus-AI, Nexus Cloud, Nexus Vault

system: Nexus-Deploy
owns: self-hosted deploy pipeline, build orchestration, runtime routing integration
depends_on: Nexus Cloud, Nexus Vault
exposes: deployment api, webhook intake, build orchestration
integrates_with: Nexus Cloud, Nexus Hosting, Nexus Network

system: Nexus-Hosting
owns: federated hosting plane, edge proxy and sync, site deployment lifecycle
depends_on: Nexus Cloud, Nexus Vault
exposes: hosting api, federation sync surfaces, site runtime surfaces
integrates_with: Nexus Cloud, Nexus-Deploy, Nexus Network

system: Nexus-Network
owns: federation telemetry dashboard, health and topology observability
depends_on: Nexus Cloud
exposes: telemetry dashboard, network status surfaces
integrates_with: Nexus Cloud, Nexus Hosting, Nexus-Deploy, Nexus

system: Nexus-Porter
owns: port intelligence and runtime diagnostics for host machines
depends_on: Nexus Cloud
exposes: port scan and diagnostics surfaces
integrates_with: Nexus Cloud, Nexus-Computer

system: Nexus-Vault
owns: encrypted secret storage, runtime secret resolution, auditability surfaces
depends_on: Nexus Cloud
exposes: vault api, secret retrieval surfaces
integrates_with: Nexus Cloud, Nexus-Deploy, Nexus-Hosting, Nexus-Computer, Nexus-AI

system: Nexusclaw
owns: local-first multi-agent orchestration framework
depends_on: Nexus-AI, Nexus Cloud
exposes: orchestrator runtime, agent gateway, tool ecosystem interfaces
integrates_with: Nexus-AI, Nexus Cloud, Nit

system: Nit
owns: cross-repo test runner, ecosystem verification workflow
depends_on: Nexus Cloud
exposes: run and audit orchestration surfaces
integrates_with: Nexusclaw, Nexus Cloud

system: Phantom
owns: anonymous networking protocol primitives, simulation and privacy-preserving research surfaces
depends_on: Nexus Cloud
exposes: protocol and simulation crates
integrates_with: Nexus Cloud, Nexus Network

Directory map

Each tool lives in its own folder:
[Nexus](Nexus) [Nexus-AI](Nexus-AI) [Nexus-Cloud](Nexus-Cloud) [Nexus-Computer](Nexus-Computer) [Nexus-Deploy](Nexus-Deploy) [Nexus-Hosting](Nexus-Hosting) [Nexus-Network](Nexus-Network) [Nexus-Porter](Nexus-Porter) [Nexus-Vault](Nexus-Vault) [Nexusclaw](Nexusclaw) [Nit](Nit) [Phantom](Phantom)

These are standalone products and integrated ecosystem participants.

---

# Nexus UI Intelligence Doctrine (Implementation Spec)

This section defines how AI should evaluate and improve UI quality across the Nexus Systems ecosystem.

## Operating model

- Nit is the cross-repo UI audit runner.
- Nexus Cloud is the canonical host-shell reference.
- Each tool produces a UI fingerprint artifact.
- AI review consumes fingerprint + visual diffs and returns actionable fixes.

## Non-negotiable policy rules

- Browser-first accessibility is mandatory.
- Cloud-hosted tools must conform to the Nexus shell contract.
- No tool can bypass design tokens for core surfaces.
- All critical flows must pass visual and accessibility checks before release.

## Exact design token schema (v1)

Canonical token file contract: `tokens/nexus.tokens.json`

```json
{
	"$schema": "https://nexus.systems/schemas/design-tokens.v1.json",
	"version": "1.0.0",
	"theme": {
		"name": "nexus-cloud-default",
		"mode": "dark"
	},
	"color": {
		"bg": { "canvas": "#090d0d", "surface": "#0f1616", "elevated": "#142020" },
		"text": { "primary": "#e6f2ee", "secondary": "#9db4ad", "muted": "#7d948d", "inverse": "#0a0f0f" },
		"accent": { "primary": "#27c9a5", "primaryHover": "#1fb592", "primaryActive": "#189f80" },
		"state": { "success": "#2ac57d", "warning": "#e8b24a", "danger": "#e15d5d", "info": "#5ca8ff" },
		"border": { "subtle": "rgba(230,242,238,0.12)", "strong": "rgba(230,242,238,0.28)" }
	},
	"typography": {
		"fontFamily": {
			"base": "IBM Plex Sans, Segoe UI, sans-serif",
			"mono": "IBM Plex Mono, ui-monospace, monospace"
		},
		"size": { "xs": 12, "sm": 14, "md": 16, "lg": 20, "xl": 28 },
		"weight": { "regular": 400, "medium": 500, "semibold": 600, "bold": 700 },
		"lineHeight": { "tight": 1.2, "normal": 1.45, "relaxed": 1.7 }
	},
	"space": { "0": 0, "1": 4, "2": 8, "3": 12, "4": 16, "5": 20, "6": 24, "8": 32, "10": 40, "12": 48 },
	"radius": { "sm": 6, "md": 10, "lg": 14, "pill": 999 },
	"shadow": {
		"sm": "0 2px 8px rgba(0,0,0,0.18)",
		"md": "0 8px 24px rgba(0,0,0,0.24)",
		"lg": "0 16px 40px rgba(0,0,0,0.32)"
	},
	"motion": {
		"duration": { "fast": 120, "normal": 180, "slow": 280 },
		"easing": { "standard": "cubic-bezier(0.2,0,0,1)", "emphasized": "cubic-bezier(0.2,0,0,0.8)" }
	},
	"zIndex": { "base": 1, "header": 50, "overlay": 100, "modal": 200, "toast": 300 }
}
```

Required rules:

- Core layout surfaces must reference tokens only (no hardcoded hex/rgb values).
- Tool-specific branding may only override `color.accent.*` and optional banner imagery.
- Contrast floors: text-to-background must satisfy WCAG AA minimum.

## Nexus Cloud shell contract (hosted tools)

When rendered in Nexus Cloud, tools must conform to this shell contract:

- Shell regions: `app-header`, `app-sidebar`, `app-content`, `app-utility-rail`.
- Tool content is mounted only inside `app-content`.
- Header height, sidebar width, nav spacing, and interaction states are token-driven.
- Shared primitives are mandatory: button, input, select, modal, toast, tabs, table, command-palette.
- Keyboard support is mandatory for shell and tool navigation.
- Responsive breakpoints (required): `360`, `768`, `1024`, `1280`, `1536`.
- In cloud mode, tools cannot inject global CSS resets or override shell root variables except approved accent tokens.

## UI fingerprint artifact schema (per tool)

Artifact path:

- `artifacts/ui-fingerprint/<tool-name>/fingerprint.json`

Required sections:

- tokenUsageMap: token keys used, missing, and non-token style violations.
- componentUsageMap: counts of shared primitives and custom components.
- screenshotSet: viewport/theme/state captures with file hashes.
- accessibilityReport: violations, severity, impacted routes, axe rule ids.
- computedStyleSummary: extracted top-level typography, spacing, contrast, and layout metrics.

Minimal shape:

```json
{
	"tool": "nexus-ai",
	"commit": "<sha>",
	"generatedAt": "<iso8601>",
	"tokenUsageMap": { "used": [], "missing": [], "violations": [] },
	"componentUsageMap": { "shared": {}, "custom": {} },
	"screenshotSet": [],
	"accessibilityReport": { "violations": [], "passes": 0 },
	"computedStyleSummary": { "routes": [] }
}
```

## Screenshot matrix (required coverage)

For each critical route in each tool:

- Viewports: `360x800`, `768x1024`, `1366x768`.
- Themes: default cloud theme + tool accent variant.
- States: default, hover, focus-visible, loading, error, empty, populated.
- Shell modes: standalone and cloud-hosted (for tools that support both).

Minimum set per tool:

- Auth surface
- Main dashboard/listing
- Primary creation/edit flow
- Detail page
- Error and empty state

## Scoring rubric (release quality)

Total score: 100

- Visual consistency: 30
- Accessibility: 30
- Interaction quality: 15
- Responsive integrity: 15
- Performance and stability signals: 10

Status thresholds:

- Green: `>= 85`
- Yellow: `70-84`
- Red: `< 70`

Automatic fail conditions regardless of total score:

- Any critical accessibility violation
- Shell contract break in cloud-hosted mode
- Token bypass on core surfaces
- Missing screenshot matrix coverage for critical routes

## CI gate design

Primary orchestrator: Nit

Pipeline stages:

1. Build and run target tool
2. Capture screenshot matrix
3. Run accessibility scan
4. Run token and component contract checks
5. Compute fingerprint artifact
6. Diff against baseline fingerprint and baseline screenshots
7. Run AI reviewer on fingerprint + diffs
8. Publish scorecard and gate result

Gate outputs:

- `artifacts/ui-fingerprint/<tool>/fingerprint.json`
- `artifacts/ui-fingerprint/<tool>/scorecard.json`
- `artifacts/ui-fingerprint/<tool>/ai-review.md`
- `artifacts/ui-fingerprint/<tool>/diff-report.json`

Merge policy:

- Block merge on red status or any automatic fail condition.
- Require explicit maintainer override note for yellow status.

## AI reviewer contract

Inputs:

- current fingerprint
- previous baseline fingerprint
- screenshot diffs
- shell contract definitions
- token schema

Required AI output:

- prioritized issue list (`critical`, `high`, `medium`, `low`)
- root-cause hypothesis per issue
- exact file-level change suggestions
- expected post-fix score delta

## Rollout plan

Phase 1 (immediate):

- Introduce token schema and shell contract docs.
- Enable fingerprint generation for Nexus Cloud + Nexus-AI + Nexus-Computer.

Phase 2:

- Expand to Nexus-Deploy, Nexus-Hosting, Nexus-Network, Nexus-Vault.
- Enforce CI gate on pull requests for cloud-hosted routes.

Phase 3:

- Full ecosystem coverage including standalone and cloud-hosted parity checks.
- Add trend dashboards in Nexus Cloud for UI score history.

