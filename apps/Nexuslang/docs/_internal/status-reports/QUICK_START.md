# NexusLang Quick Start Guide

Get NexusLang running in **5 minutes** - choose your installation method.

## Prerequisites

- **Python 3.11+** (required)
- **VSCode** (recommended) or any editor
- **pip** (Python package manager)

Check your Python version:
```bash
python3 --version # Should be 3.11 or higher
```

---

## Installation Methods

### Method 1: Quick Install (Recommended)

**For developers in this repository:**

```bash
# 1. Install NexusLang globally
./install_extension_globally.sh

# 2. Reload VSCode
# Press Ctrl+Shift+P "Developer: Reload Window"

# 3. Open any .nlpl file - extension activates automatically!
code examples/01_basic_concepts.nlpl
```

**For new projects:**

```bash
# 1. Clone or navigate to your project
cd your-project/

# 2. Copy extension files
cp -r /path/to/NexusLang/.vscode/extensions/nlpl .vscode/extensions/

# 3. Configure workspace
cat > .vscode/settings.json << 'EOF'
{
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "/path/to/NexusLang/src/nxl_lsp.py"
}
EOF

# 4. Reload VSCode and open .nlpl files
```

### Method 2: Install from Package (.vsix)

**Coming soon - for end users:**

```bash
# 1. Download latest release
wget https://github.com/Zajfan/NexusLang/releases/latest/download/nlpl-extension.vsix

# 2. Install in VSCode
code --install-extension nlpl-extension.vsix

# 3. Reload VSCode
```

### Method 3: From VSCode Marketplace

**Future (once published):**

1. Open VSCode
2. Go to Extensions (Ctrl+Shift+X)
3. Search "NexusLang"
4. Click Install
5. Reload VSCode

---

## Your First NexusLang Program

### Step 1: Create a file

```bash
touch hello.nlpl
```

### Step 2: Write your code

```nlpl
# hello.nlpl - Your first NexusLang program

# Variables are natural
set name to "World"
set count to 5

# Functions read like English
function greet that takes person as String returns String
 return "Hello, " plus person plus "!"

# Call functions naturally
set message to call greet with name
print text message

# Loops are intuitive
for each num in range from 1 to count
 print text "Count: " plus num
```

### Step 3: Run it!

```bash
# Option 1: Direct interpretation
python3 src/main.py hello.nlpl

# Option 2: Using CLI (once installed)
nlpl run hello.nlpl

# Expected output:
# Hello, World!
# Count: 1
# Count: 2
# Count: 3
# Count: 4
# Count: 5
```

---

## Features in Action

### Auto-Completion

Type in VSCode and press `Ctrl+Space`:

```nlpl
set x to # Ctrl+Space shows suggestions

func # Auto-completes to "function"

for # Suggests "for each" pattern
```

### Real-Time Errors

VSCode shows errors as you type:

```nlpl
set x to "unclosed string # Error: Unclosed string
set y to undefined_var # Warning: Undefined variable
```

### Quick Fixes

Click the lightbulb or press `Ctrl+.` to fix issues:

- Fix unclosed strings automatically
- Remove unused variables
- Add type annotations
- Extract code to function

### Signature Help

Get parameter hints while typing:

```nlpl
function calculate that takes x as Integer and y as Integer returns Integer
 return x plus y

set result to call calculate with # Shows: (x: Integer, y: Integer)
```

### Go to Definition

Press `F12` on any symbol:

```nlpl
function helper returns String
 return "help"

set text to call helper # F12 on "helper" jumps to function
 # F12 here
```

---

## Example Programs

Explore examples in the `examples/` directory:

```bash
# Basic concepts
python3 src/main.py examples/01_basic_concepts.nlpl

# Object-oriented programming
python3 src/main.py examples/02_object_oriented.nlpl

# Functional programming
python3 src/main.py examples/06_functional_programming.nlpl

# Network programming
python3 src/main.py examples/04_network_programming.nlpl
```

---

## Configuration

### Workspace Settings

Create `.vscode/settings.json` in your project:

```json
{
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "/custom/path/to/nxl_lsp.py",
 "nexuslang.trace.server": "off"
}
```

### Trace Levels

Debug LSP issues with verbose logging:

```json
{
 "nexuslang.trace.server": "verbose"
}
```

Then check: `View Output "NLPL Language Server"`

### File Associations

Auto-detect NexusLang files:

```json
{
 "files.associations": {
 "*.nxl": "nlpl"
 }
}
```

---

## Troubleshooting

### Extension Not Working?

1. **Check LSP server status:**
 - Open: `View Output "NLPL Language Server"`
 - Should see: "NLPL Language Server started"

2. **Verify Python version:**
 ```bash
 python3 --version # Must be 3.11+
 ```

3. **Reload VSCode:**
 - Press `Ctrl+Shift+P`
 - Type "Reload Window"
 - Press Enter

4. **Check extension is installed:**
 - Open Extensions (Ctrl+Shift+X)
 - Search "NexusLang"
 - Should show "Installed"

### No Auto-Completion?

1. Ensure file has `.nlpl` extension
2. Check language mode (bottom-right) shows "NexusLang"
3. Try manual trigger: `Ctrl+Space`
4. Check for errors in Problems panel: `Ctrl+Shift+M`

### Custom Installation Path?

If NexusLang is not in the default location:

1. Find LSP server:
 ```bash
 find ~ -name "nxl_lsp.py" 2>/dev/null
 ```

2. Update settings:
 ```json
 {
 "nexuslang.languageServer.path": "/your/path/to/nxl_lsp.py"
 }
 ```

### Permission Issues?

```bash
# Make scripts executable
chmod +x install_extension_globally.sh
chmod +x package_extension.sh
chmod +x setup_vscode_extension.sh

# Ensure LSP server is accessible
chmod +x src/nxl_lsp.py
```

---

## Next Steps

### Learn NexusLang

1. **Language Guide:** `docs/2_language_basics/`
2. **Examples:** `examples/` directory (25+ examples)
3. **Style Guide:** `docs/7_development/style_guide.md`
4. **Roadmap:** `ROADMAP.md`

### Editor Setup

- **VSCode:** Already set up! 
- **Neovim:** See `DEPLOYMENT_GUIDE.md` Section 2.1
- **Vim:** See `DEPLOYMENT_GUIDE.md` Section 2.1
- **Sublime Text:** See `DEPLOYMENT_GUIDE.md` Section 2.2
- **Emacs:** See `DEPLOYMENT_GUIDE.md` Section 2.3

### Development

1. **Run tests:**
 ```bash
 pytest tests/
 ```

2. **Add standard library:**
 ```bash
 # Create new module
 touch src/nlpl/stdlib/your_module.py
 
 # Register in src/nlpl/stdlib/__init__.py
 ```

3. **Extend parser:**
 ```bash
 # See: docs/7_development/parser_extension_guide.md
 ```

---

## Example: Complete Program

```nlpl
# calculator.nlpl - A simple calculator

class Calculator
 property result as Float
 
 constructor
 set result to 0.0
 
 method add that takes value as Float
 set result to result plus value
 
 method subtract that takes value as Float
 set result to result minus value
 
 method multiply that takes value as Float
 set result to result times value
 
 method divide that takes value as Float
 if value is equal to 0.0
 print text "Error: Division by zero"
 return
 set result to result divided by value
 
 method get_result returns Float
 return result

# Create calculator
set calc to create Calculator

# Perform calculations
call add on calc with 10.0
call multiply on calc with 5.0
call subtract on calc with 3.0

# Get result
set final to call get_result on calc
print text "Result: " plus final # Output: Result: 47.0
```

**Run it:**

```bash
python3 src/main.py calculator.nlpl
```

---

## Get Help

- **Issues:** https://github.com/Zajfan/NexusLang/issues
- **Discussions:** https://github.com/Zajfan/NexusLang/discussions
- **Documentation:** `docs/` directory
- **Examples:** `examples/` directory

---

## Cheat Sheet

### Common Patterns

```nlpl
# Variables
set name to "value"
set numbers to list of 1, 2, 3

# Functions
function name that takes x as Integer returns String
 return "result"

# Conditionals
if x is greater than 5
 print text "big"
else
 print text "small"

# Loops
for each item in collection
 print text item

while counter is less than 10
 set counter to counter plus 1

# Classes
class Name
 property field as Type
 
 constructor that takes param as Type
 set field to param
 
 method do_something returns Type
 return field

# Create objects
set obj to create Name with value
call do_something on obj
```

### Operators

| Natural Language | Meaning |
|-----------------|---------|
| `plus` | + |
| `minus` | - |
| `times` | * |
| `divided by` | / |
| `is equal to` | == |
| `is not equal to` | != |
| `is greater than` | > |
| `is less than` | < |
| `is greater than or equal to` | >= |
| `is less than or equal to` | <= |
| `and` | && |
| `or` | \|\| |
| `not` | ! |

---

**Happy coding in NLPL!** 
