# Nexus Ecosystem Phase Rollout Board

Generated: 2026-04-27
Source of truth: Nexus Systems Ecosystem Blueprint v1.2 plus current apps directory

## Purpose

This board turns the ecosystem inventory into implementation waves.

- `P0` means ecosystem-enabling and platform-critical.
- `P1` means broad product value and daily-use surfaces.
- `P2` means specialized, vertical, or long-tail expansion.

## Status Legend

- `existing-core`: app directory existed before scaffold generation and is part of the core platform set.
- `existing-extra`: app directory existed before scaffold generation but sits outside the original scaffold mapping.
- `scaffold`: generated skeleton waiting for implementation.
- `cloud-internal`: implemented as a subsystem inside Nexus-Cloud (no separate runtime target).

## Recommended Wave Order

1. Stabilize existing `P0` apps and shared contracts.
2. Bring `P0` scaffold apps online with minimal health/API shells.
3. Roll out `P1` collaboration, business, and data surfaces.
4. Expand into `P2` creative, lifestyle, gaming, and vertical products.

## P0 Foundation And Ecosystem Enablers

### Core Platform And Sovereign Infrastructure

- `Nexus` — `existing-core`
- `Nexus-Cloud` — `existing-core`
- `Nexus-Hosting` — `existing-core`
- `Nexus-Deploy` — `existing-core`
- `Nexus-Computer` — `existing-core`
- `Nexus-Vault` — `existing-core`
- `Nexus-Network` — `existing-core`
- `Nexus-Tunnel` — `scaffold`
- `Nexus-Systems-API` — `cloud-internal`
- `Nexus-Edge` — `scaffold`
- `Phantom` — `existing-extra`

### Security, Identity, Guardrails, And Operations

- `Nexus-Guardian` — `scaffold`
- `Nexus-Auth` — `scaffold`
- `Nexus-Monitor` — `scaffold`
- `Nexus-Security` — `scaffold`
- `Nexus-Compliance` — `scaffold`
- `Nexus-Terminal` — `scaffold`

### AI Runtime, Agentic Control, And Dev Platform

- `Nexus-AI` — `existing-extra`
- `Nexusclaw` — `existing-extra`
- `Nexus-AI-Hub` — `scaffold`
- `Nexus-Agents` — `scaffold`
- `Nexus-Inference` — `scaffold`
- `Nexus-Forge` — `existing-extra`
- `Nexus-IDE` — `scaffold`
- `Nexus-API` — `scaffold`
- `Nexus-Testing` — `scaffold`
- `Nexus-Code-Review` — `scaffold`
- `Nexuslang` — `scaffold`

### Foundational Data And Discovery Surfaces

- `Nexus-Files` — `scaffold`
- `Nexus-Search` — `scaffold`

## P1 Broad Product Surface

### Personal Knowledge, Productivity, And Internal Ops

- `Nexus-Photos` — `scaffold`
- `Nexus-Book` — `scaffold`
- `Nexus-Notes` — `scaffold`
- `Nexus-Tasks` — `scaffold`
- `Nexus-Calendar` — `scaffold`
- `Nexus-Office` — `scaffold`
- `Nexus-Automate` — `scaffold`
- `Nexus-Forms` — `scaffold`
- `Nexus-Wiki` — `scaffold`
- `Nexus-Planner` — `scaffold`
- `Nexus-PDF` — `scaffold`

### Communication And Collaboration Layer

- `Nexus-Meet` — `scaffold`
- `Nexus-Email` — `scaffold`
- `Nexus-Team-Chat` — `scaffold`
- `Nexus-Presence` — `scaffold`

### Business, Revenue, And Admin Systems

- `Nexus-CRM` — `scaffold`
- `Nexus-Sales` — `scaffold`
- `Nexus-HR` — `scaffold`
- `Nexus-Accounting` — `scaffold`
- `Nexus-Contracts` — `scaffold`
- `Nexus-Billing` — `scaffold`

### Data, Intelligence, And Reporting

- `Nexus-Analytics` — `scaffold`
- `Nexus-Insights` — `scaffold`
- `Nexus-Survey` — `scaffold`
- `Nexus-SEO` — `scaffold`
- `Nexus-Database` — `scaffold`

### Learning, Commerce, And Trust Extensions

- `Nexus-Learn` — `scaffold`
- `Nexus-Tutor` — `scaffold`
- `Nexus-Academy` — `scaffold`
- `Nexus-Store` — `scaffold`
- `Nexus-Market` — `scaffold`
- `Nexus-Provenance` — `scaffold`
- `Nexus-Confidential` — `scaffold`

## P2 Specialized, Vertical, And Long-Tail Expansion

### Media, Creativity, And Content Production

- `Nexus-Media` — `scaffold`
- `Nexus-Music` — `scaffold`
- `Nexus-Radio-Live` — `scaffold`
- `Nexus-Modeling` — `scaffold`
- `Nexus-Draw` — `scaffold`
- `Nexus-Video` — `scaffold`
- `Nexus-Graphic` — `scaffold`
- `Nexus-Design` — `scaffold`
- `Nexus-Content` — `scaffold`

### Personal Life, Community, And Wellness

- `Nexus-Finance` — `scaffold`
- `Nexus-Home` — `scaffold`
- `Nexus-Social` — `scaffold`
- `Nexus-Health` — `scaffold`
- `Nexus-Fitness` — `scaffold`
- `Nexus-Nutrition` — `scaffold`
- `Nexus-Mind` — `scaffold`

### Gaming, Simulation, And Sector Modules

- `Nexus-Game` — `scaffold`
- `Nexus-Play` — `scaffold`
- `Nexus-Arcade` — `scaffold`
- `Nexus-Engine` — `scaffold`
- `Nexus-Vertical` — `scaffold`
- `Nexus-Spend` — `scaffold`

## Immediate Execution Guidance

1. Treat every `P0` scaffold as a contract-first shell: health route, README scope, minimal service entrypoint, Systems API registration.
2. Use `P1` to form the first coherent product bundles: collaboration, productivity, business, and analytics.
3. Leave `P2` mostly in scaffold state until the platform, auth, AI runtime, routing, and shared data contracts are stable.
4. Treat `Nexus-Systems-API` as a `Nexus-Cloud` internal stream and ship specs/SDK artifacts from its scaffold directory without creating a separate runtime.