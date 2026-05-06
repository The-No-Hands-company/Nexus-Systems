# NexusLang Language Server Installation

The NexusLang Language Server provides IDE features like autocomplete, go-to-definition, and diagnostics for NexusLang code.

## Installation

### Option 1: Run from Source (Development)

```bash
# From project root
python -m nexuslang.lsp

# With debug logging
python -m nexuslang.lsp --debug --log-file /tmp/nlpl-lsp.log
```

### Option 2: Standalone Script

```bash
# From project root
./scripts/nlpl-lsp

# Or add to PATH
export PATH="$PATH:/path/to/NexusLang/scripts"
nlpl-lsp
```

### Option 3: Install Globally (Recommended for Editors)

```bash
# Create a symlink in a directory on your PATH
sudo ln -s /path/to/NexusLang/scripts/nlpl-lsp /usr/local/bin/nlpl-lsp

# Verify installation
which nlpl-lsp
nlpl-lsp --help
```

## Editor Configuration

### VS Code

The NexusLang VS Code extension automatically uses the LSP server. No additional configuration needed.

### Neovim (nvim-lspconfig)

Add to your `init.lua`:

```lua
local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Define NexusLang LSP
if not configs.nlpl then
  configs.nlpl = {
    default_config = {
      cmd = {'nlpl-lsp'},  -- or {'python', '-m', 'nexuslang.lsp'}
      filetypes = {'nlpl'},
      root_dir = lspconfig.util.root_pattern('.git', 'nexuslang.toml'),
      settings = {},
    },
  }
end

-- Enable NexusLang LSP
lspconfig.nlpl.setup{}
```

### Vim (vim-lsp)

Add to your `.vimrc`:

```vim
if executable('nlpl-lsp')
  augroup LspNLPL
    autocmd!
    autocmd User lsp_setup call lsp#register_server({
      \ 'name': 'nlpl-lsp',
      \ 'cmd': {server_info->['nlpl-lsp']},
      \ 'whitelist': ['nlpl'],
      \ })
  augroup END
endif
```

### Emacs (lsp-mode)

Add to your `init.el`:

```elisp
(with-eval-after-load 'lsp-mode
  (add-to-list 'lsp-language-id-configuration '(nlpl-mode . "nlpl"))
  (lsp-register-client
   (make-lsp-client
    :new-connection (lsp-stdio-connection "nlpl-lsp")
    :major-modes '(nlpl-mode)
    :server-id 'nlpl-lsp)))
```

### Sublime Text (LSP)

Add to LSP settings (`Preferences > Package Settings > LSP > Settings`):

```json
{
  "clients": {
    "nlpl": {
      "enabled": true,
      "command": ["nlpl-lsp"],
      "selector": "source.nxl"
    }
  }
}
```

## Features

The NexusLang Language Server provides:

- **Completions**: Context-aware code completion for keywords, functions, classes, and variables
- **Diagnostics**: Real-time syntax and semantic error checking
- **Go to Definition**: Navigate to function, class, and variable definitions
- **Hover**: Type information and documentation on hover
- **Symbols**: Workspace and document symbol navigation
- **Formatting**: Code formatting with consistent style
- **Rename**: Safe symbol renaming across workspace
- **References**: Find all references to a symbol
- **Signature Help**: Function parameter hints
- **Code Actions**: Quick fixes and refactorings

## Troubleshooting

### Enable Debug Logging

```bash
nlpl-lsp --debug --log-file ~/nlpl-lsp.log
```

Then check the log file for errors.

### Test LSP Connection

```bash
# Send initialize request
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}' | nlpl-lsp
```

### Common Issues

**Issue**: `nlpl-lsp: command not found`
- **Solution**: Ensure the script is in your PATH or use the full path

**Issue**: LSP not starting in editor
- **Solution**: Check editor LSP logs and verify `nlpl-lsp` runs from terminal

**Issue**: No completions/diagnostics
- **Solution**: Ensure NexusLang syntax is valid, check LSP logs for errors

## Requirements

- Python 3.10 or higher
- NexusLang source code and dependencies installed

## Support

For issues or questions:
- GitHub Issues: https://github.com/Zajfan/NexusLang/issues
- Documentation: See `docs/` directory in the project

## License

Same as NexusLang project - see LICENSE file in project root.
