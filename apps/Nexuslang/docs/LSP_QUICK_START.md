# NexusLang LSP Quick Start Guide

## Install Extension (30 seconds)

```bash
code --install-extension vscode-extension/nlpl-language-support-0.1.0.vsix
```

Then reload VS Code: `Ctrl+Shift+P` → "Reload Window"

## Verify It Works (1 minute)

1. Open `examples/01_basic_concepts.nlpl`
2. Check View → Output → "NLPL Language Server" (should see startup message)
3. Type `func` + `Ctrl+Space` (should see completions)
4. Hover over `print` (should see docs)

## If Problems

**Extension not loading?**
```bash
# Check Python
python3 --version

# Check LSP runs
python -m nexuslang.lsp --help

# Check VS Code Output panel (View → Output → NexusLang Language Server)
```

**Enable debug logging:**
File → Preferences → Settings → search "nlpl" → check "Debug" box

## Run Full Tests

See [LSP_MANUAL_TEST_RESULTS.md](_internal/status-reports/LSP_MANUAL_TEST_RESULTS.md)

10 features to test:
1. Syntax highlighting
2. Diagnostics
3. Auto-completion
4. Go-to-definition
5. Find references
6. Rename refactoring
7. Hover documentation
8. Code actions
9. Signature help
10. Formatting

## Report Issues

Document in LSP_MANUAL_TEST_RESULTS.md:
- ✅ Works perfectly
- ⚠️ Partially working (describe issue)
- ❌ Broken (describe what happens)

---

**That's it! Now you have a working LSP extension.**
