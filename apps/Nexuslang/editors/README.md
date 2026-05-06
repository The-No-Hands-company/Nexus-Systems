# NexusLang Editor Integrations

This directory contains editor plugins and IDE extensions for NexusLang.

| Editor | Location | Status |
|--------|----------|--------|
| Neovim | `neovim/` | Complete |
| Emacs | `emacs/` | Complete |
| Sublime Text | `sublime-text/` | Complete |
| IntelliJ IDEA (and JetBrains IDEs) | `intellij/` | Complete |
| VS Code | `../vscode-extension/` | Separate directory |

Why VS Code is separate:
- The VS Code integration is a standalone Node/TypeScript extension project (`package.json`, build/test tooling, VSIX packaging).
- Keeping it at the repository root avoids coupling editor-agnostic assets under `editors/` with extension build artifacts and release tooling.
- `editors/` is kept for lightweight editor configs/modes; `vscode-extension/` remains the canonical source for the VS Code marketplace extension.

## Quick Install

### Neovim (lazy.nvim)
```lua
{
  dir = "path/to/NexusLang/editors/neovim",
  config = function()
    require("nlpl").setup()
  end
}
```

### Emacs
```elisp
(load "path/to/NexusLang/editors/emacs/nlpl-mode.el")
```

### Sublime Text
Copy `sublime-text/NexusLang/` into your Sublime Text `Packages/` directory.

### IntelliJ IDEA
Build with `./gradlew buildPlugin` and install from disk, or wait for the JetBrains Marketplace release.

All integrations connect to the NexusLang language server (`nlpl lsp`) for:
- Code completion
- Go-to-definition
- Find references
- Hover documentation
- Inline diagnostics
- Signature help
