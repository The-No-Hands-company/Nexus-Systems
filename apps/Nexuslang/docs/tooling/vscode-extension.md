# NexusLang VS Code Extension Installation & Usage Guide

## Installation

### Method 1: Install from .vsix file

1. Download the latest release: `nlpl-language-support-0.1.0.vsix`
2. Open VS Code
3. Go to Extensions view (Ctrl+Shift+X)
4. Click "..." menu → "Install from VSIX..."
5. Select the downloaded .vsix file
6. Reload VS Code when prompted

### Method 2: Command line installation

```bash
code --install-extension nlpl-language-support-0.1.0.vsix
```

### Method 3: VS Code Marketplace (Future)

Once published, search for "NexusLang" in the Extensions marketplace.

## Prerequisites

The NexusLang language server must be installed and accessible:

```bash
# Option 1: System-wide installation
pip install nlpl-compiler

# Option 2: Development installation
cd /path/to/NLPL
pip install -e .

# Verify installation
python -m nexuslang.lsp --help
```

## Features

### 1. Syntax Highlighting

Full TextMate grammar support for NexusLang syntax:
- Keywords: `function`, `class`, `if`, `while`, `for`, `return`, `end`
- Types: `Integer`, `Float`, `String`, `Boolean`, `List`, `Dict`
- Operators: `plus`, `minus`, `equals`, `is greater than`
- Comments: `# line comments`
- Strings: Double and single quoted
- Numbers: Integers, floats, hex, binary

### 2. Semantic Highlighting

AST-based semantic tokens provide accurate highlighting based on symbol meaning:
- Function declarations vs function calls (different colors)
- Class names vs instances
- Parameters vs local variables
- Properties vs methods
- Constants vs mutable variables

**Enable in VS Code:**
```json
{
  "editor.semanticHighlighting.enabled": true
}
```

### 3. IntelliSense

Code completion with context-aware suggestions:
- Keywords (function, class, if, etc.)
- Standard library functions (print, length, etc.)
- Imported symbols
- Class members
- Function parameters

**Trigger:** Type or press `Ctrl+Space`

### 4. Go to Definition

Jump to symbol definitions:
- Functions
- Classes
- Variables
- Methods
- Imports

**Usage:**
- Right-click → "Go to Definition"
- F12
- Ctrl+Click (hold Ctrl and click symbol)

### 5. Find All References

Find all usages of a symbol:
- Shows definition and all references
- Works across files
- Lists results in sidebar

**Usage:**
- Right-click → "Find All References"
- Shift+F12

### 6. Rename Symbol

Safe renaming across entire workspace:
- Renames definition and all references
- Preview changes before applying
- Works across files

**Usage:**
- Right-click → "Rename Symbol"
- F2
- Preview changes → Accept or reject

### 7. Hover Information

Show information on hover:
- Function signatures
- Type information
- Documentation (if available)

**Usage:** Hover mouse over symbol

### 8. Document Symbols

Outline view of file structure:
- Functions
- Classes
- Methods
- Variables

**Usage:**
- Ctrl+Shift+O (Go to Symbol in File)
- View → Outline panel

### 9. Workspace Symbols

Search symbols across entire workspace:
- Fuzzy matching
- All symbol types
- Quick navigation

**Usage:**
- Ctrl+T (Go to Symbol in Workspace)
- Type symbol name

### 10. Code Actions

Quick fixes and refactorings:

**Organize Imports:**
- Right-click → "Source Action..." → "Organize Imports"
- Sorts imports alphabetically
- Removes duplicates

**Extract Function:**
1. Select code to extract
2. Right-click → "Refactor..." → "Extract Function"
3. Enter function name
4. Function created above current location

**Add Type Annotation:**
- Cursor on variable without type
- Light bulb appears
- Click → "Add type annotation"

**Declare Variable:**
- On undefined variable error
- Light bulb appears
- Click → "Declare '[name]'"

### 11. Diagnostics

Real-time error and warning reporting:
- Syntax errors
- Type errors
- Undefined symbols
- Unused variables

**Display:**
- Errors shown inline (red squiggles)
- Problems panel (Ctrl+Shift+M)
- Status bar error count

## Configuration

### Extension Settings

Open VS Code settings (Ctrl+,) and search for "nlpl":

```json
{
  // Enable/disable language server
  "nexuslang.languageServer.enabled": true,
  
  // Path to language server (leave empty for auto-detect)
  "nexuslang.languageServer.path": "",
  
  // Server arguments
  "nexuslang.languageServer.arguments": ["--stdio"],
  
  // Enable debug mode
  "nexuslang.languageServer.debug": false,
  
  // Log file path
  "nexuslang.languageServer.logFile": "/tmp/nlpl-lsp.log",
  
  // Trace server communication
  "nexuslang.trace.server": "off"  // "off", "messages", or "verbose"
}
```

### Custom Language Server Path

If NexusLang is not in your PATH:

```json
{
  "nexuslang.languageServer.path": "/path/to/nlpl-lsp"
}
```

### Debug Mode

To troubleshoot issues:

```json
{
  "nexuslang.languageServer.debug": true,
  "nexuslang.trace.server": "verbose",
  "nexuslang.languageServer.logFile": "/tmp/nlpl-debug.log"
}
```

Then check the log file for details.

## Example Usage

### Create a new NexusLang file

1. Create file: `hello.nlpl`
2. Type:

```nexuslang
function greet with name as String returns Nothing
    print text "Hello, " plus name
end

greet with "World"
```

3. Enjoy:
- Syntax highlighting
- IntelliSense as you type
- Go to definition on `greet`
- Hover over `greet` for signature
- Semantic highlighting distinguishes function definition from call

### Navigate a project

```nexuslang
# file: math_utils.nlpl
function add with x as Integer and y as Integer returns Integer
    return x plus y
end

function multiply with x as Integer and y as Integer returns Integer
    return x times y
end

# file: main.nlpl
import math_utils

set result to math_utils.add with 10 and 20
print text result
```

**Usage:**
- Ctrl+T → Type "add" → Jump to definition
- Ctrl+Shift+O → View outline of current file
- F12 on `math_utils.add` → Jump to function in other file
- Shift+F12 on `add` → See all usages

### Refactor code

```nexuslang
# Before
set x to 10
set y to 20
set sum to x plus y
set product to x times y
print text sum
print text product
```

**Extract function:**
1. Select lines with sum calculation
2. Right-click → Refactor → Extract Function
3. Name: `calculate_sum`
4. Result:

```nexuslang
function calculate_sum returns Nothing
    set sum to x plus y
    print text sum
end

set x to 10
set y to 20
calculate_sum
set product to x times y
print text product
```

## Troubleshooting

### Extension not activating

**Check:**
1. Open Output panel (Ctrl+Shift+U)
2. Select "NLPL Language Server" from dropdown
3. Look for errors

**Common issues:**
- Language server not installed: `pip install nlpl-compiler`
- Wrong Python path: Configure `nlpl.languageServer.path`
- Extension disabled: Check Extensions view

### No IntelliSense

**Solutions:**
1. Reload window: Ctrl+Shift+P → "Developer: Reload Window"
2. Check language mode: Bottom right corner should show "NexusLang"
3. Verify file extension is `.nlpl`
4. Enable debug mode and check logs

### Semantic highlighting not working

**Enable:**
```json
{
  "editor.semanticHighlighting.enabled": true
}
```

**Check theme:** Some themes don't support semantic tokens. Try:
- Dark+ (default dark)
- Light+ (default light)
- Monokai

### Slow performance

**Solutions:**
1. Disable semantic highlighting (large files)
2. Reduce trace level: `"nexuslang.trace.server": "off"`
3. Close unused files
4. Increase VS Code memory: `--max-memory=4096`

### Language server crashes

**Debug:**
1. Enable debug mode
2. Check log file: `/tmp/nlpl-lsp.log`
3. Report issue with log contents

## Keyboard Shortcuts

| Action | Windows/Linux | Mac |
|--------|---------------|-----|
| Go to Definition | F12 | F12 |
| Find References | Shift+F12 | Shift+F12 |
| Rename Symbol | F2 | F2 |
| Go to Symbol (File) | Ctrl+Shift+O | Cmd+Shift+O |
| Go to Symbol (Workspace) | Ctrl+T | Cmd+T |
| Show Hover | Ctrl+K Ctrl+I | Cmd+K Cmd+I |
| Trigger Completion | Ctrl+Space | Ctrl+Space |
| Show Code Actions | Ctrl+. | Cmd+. |
| Format Document | Shift+Alt+F | Shift+Option+F |

## Theme Support

The extension works with all VS Code themes. For best semantic highlighting:

**Recommended themes:**
- Dark+ (default)
- Monokai
- Solarized Dark
- One Dark Pro
- Dracula

**Custom theme colors:**
```json
{
  "editor.semanticTokenColorCustomizations": {
    "rules": {
      "function": "#DCDCAA",
      "method": "#DCDCAA",
      "class": "#4EC9B0",
      "variable": "#9CDCFE",
      "parameter": "#9CDCFE"
    }
  }
}
```

## File Associations

The extension automatically activates for `.nlpl` files.

**Manual activation:**
1. Open file
2. Click language mode (bottom right)
3. Select "NexusLang"

**Configure associations:**
```json
{
  "files.associations": {
    "*.nxl": "nlpl"
  }
}
```

## Updates

The extension auto-updates from VS Code Marketplace.

**Manual update:**
1. Extensions view
2. Find "NLPL Language Support"
3. Click "Update" if available

**Check version:**
Extensions view → NexusLang Language Support → Version shown below name

## Support

**Issues:** https://github.com/Zajfan/NexusLang/issues
**Documentation:** https://github.com/Zajfan/NexusLang/docs
**Discord:** (TBD)

## Contributing

See [CONTRIBUTING.md](https://github.com/Zajfan/NexusLang/CONTRIBUTING.md)

## License

MIT License - See LICENSE file
