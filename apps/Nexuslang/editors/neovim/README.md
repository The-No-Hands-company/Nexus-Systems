# nlpl.nvim

Neovim plugin providing full NexusLang language support.

## Features

- Syntax highlighting (regex-based, no tree-sitter required)
- LSP integration (autocompletion, go-to-definition, hover, rename, references, signature help)
- Smart indentation
- Comment string (`#`)
- Folding via indentation
- User commands: `:NLPLBuild`, `:NLPLTest`, `:NLPLRun`, `:NLPLCoverage`, `:NLPLProfile`, `:NLPLDoc`
- Optional nvim-cmp integration for richer completions

## Prerequisites

- Neovim 0.9 or later
- NexusLang installed: `pip install nlpl` (makes `nlpl` CLI and LSP server available)
- `nvim-lspconfig` plugin (for LSP features)
- `nvim-cmp` (optional, for enhanced completions)

## Installation

### lazy.nvim

```lua
{
  dir = "/path/to/NexusLang/editors/neovim",
  name = "nexuslang.nvim",
  ft = "nlpl",
  config = function()
    require("nlpl").setup()
  end,
}
```

### packer.nvim

```lua
use {
  "/path/to/NexusLang/editors/neovim",
  config = function()
    require("nlpl").setup()
  end,
}
```

### Manual installation

Copy this directory to your Neovim data path:

```bash
mkdir -p ~/.config/nvim/pack/nlpl/start/nlpl.nvim
cp -r /path/to/NexusLang/editors/neovim/* ~/.config/nvim/pack/nlpl/start/nlpl.nvim/
```

Then add to your `init.lua`:

```lua
require("nlpl").setup()
```

## Configuration

All options with defaults:

```lua
require("nlpl").setup({
  -- Path to the NexusLang LSP server __main__.py (auto-detected if nil)
  lsp_server_path = nil,

  -- Python executable used to run the LSP server
  python_cmd = "python3",

  -- Enable LSP integration (requires nvim-lspconfig)
  lsp_enable = true,

  -- Indentation width (spaces)
  indent_width = 4,

  -- Enable default key mappings (gd, K, <C-k>, <leader>rn, etc.)
  keymaps = true,

  -- Extra settings forwarded to the LSP server
  lsp_settings = {},

  -- Callback called after the NexusLang LSP server attaches to a buffer
  on_attach = nil,
})
```

## Default Key Mappings

When LSP is active and `keymaps = true`:

| Key             | Action                    |
|-----------------|---------------------------|
| `gd`            | Go to definition          |
| `K`             | Hover documentation       |
| `gi`            | Go to implementation      |
| `<C-k>`         | Signature help            |
| `<leader>rn`    | Rename symbol             |
| `<leader>ca`    | Code action               |
| `gr`            | Find references           |
| `<leader>f`     | Format buffer             |

## Commands

| Command              | Description                                  |
|----------------------|----------------------------------------------|
| `:NLPLBuild`         | `nlpl build` in terminal                     |
| `:NLPLBuild --release` | `nlpl build --release` in terminal         |
| `:NLPLTest`          | `nlpl test` in terminal                      |
| `:NLPLTest --coverage` | `nlpl test --coverage` in terminal         |
| `:NLPLRun`           | `nlpl run` in terminal                       |
| `:NLPLCoverage [file]` | `nlpl coverage` on current or given file   |
| `:NLPLProfile [file]`  | `nlpl profile` on current or given file    |
| `:NLPLDoc [dir]`     | `nlpl doc` to generate API docs              |
