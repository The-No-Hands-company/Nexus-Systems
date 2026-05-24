# VS Code Agent and Skill Setup

This document describes how to use the Nexus Modeling agent guidance in VS Code.

## Purpose

The repository already includes a Claude-native `.claude/` agent setup. This file and `copilot-instructions.md` provide the equivalent VS Code setup so you can work with GitHub Copilot in the same structured way.

## Recommended extensions

Install the following VS Code extensions for the best experience:

- `GitHub.copilot-chat`
- `GitHub.copilot`
- `ms-vscode.cpptools`
- `ms-vscode.cmake-tools`

The workspace also includes `.vscode/extensions.json` to recommend these extensions automatically.

## Workspace instructions

Use `copilot-instructions.md` as the primary VS Code Copilot instruction file. It encodes:

- agent personas and ownership boundaries
- workspace build/test conventions
- render and backend policy constraints
- workflow skills for feature delivery, hardening, and review

## Agent persona mapping

Match VS Code Copilot roles to the existing repository personas:

- `software-architect` for API design and cross-module plans
- `render-engineer` for scheduler/render path implementation
- `gpu-backend-engineer` for backend/gfx device work
- `simulation-engineer` for sim driver and solver work
- `test-engineer` for deterministic GoogleTest coverage
- `code-reviewer` for diff-level review

## Using the skills

Treat the workflows as structured tasks:

- `feature`: design → implement → test → review
- `harden`: non-finite input and API hardening
- `kreview`: focused code-review and risk analysis

## Reference docs

- `AGENTS.md` — repository operating contract
- `CLAUDE.md` — Claude workflow guidance
- `.claude/` — existing agent definitions and command workflows
- `copilot-instructions.md` — VS Code Copilot instructions

## Notes

This setup is intentionally aligned with the existing `.claude/` workflows and the repository’s codebase conventions. Use it when working in VS Code to preserve the same structured agent behavior as Claude.
