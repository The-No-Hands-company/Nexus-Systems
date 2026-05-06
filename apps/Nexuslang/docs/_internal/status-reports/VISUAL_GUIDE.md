# NexusLang Distribution & Usage - Visual Guide

## Complete Overview

```

 NexusLang ECOSYSTEM 
 
 Source LSP 
 Code Server 
 (.nxl) (nxl_lsp.py) 
 
 Language Server Protocol 
 
 VSCode Vim/ Sublime 
 Extension Neovim Text 
 
 Developer 
 Experience 
 
 Syntax Code Quick 
 Highlight Complete Fixes 
 
```

---

## Distribution Channels

### Current Setup (What You Have Now)

```
Your NexusLang Repository
 .vscode/
 extensions/nlpl/ VSCode extension (ready!)
 extension.js Language client
 package.json Extension manifest
 syntaxes/ Syntax highlighting
 node_modules/ Dependencies
 README.md Marketplace docs
 CHANGELOG.md Version history
 LICENSE MIT license
 settings.json Workspace config
 src/nlpl/lsp/ LSP server implementation
 server.py Main LSP server
 diagnostics.py Error checking
 completion.py Auto-completion
 signature_help.py Parameter hints
 code_actions.py Quick fixes
 install_extension_globally.sh Global install script 
 package_extension.sh Build .vsix packages 
 DEPLOYMENT_GUIDE.md Publishing guide 
 QUICK_START.md User tutorial 
 DISTRIBUTION_SUMMARY.md This overview 
```

### Usage Scenarios

```

 Scenario 1: Using Extension in This Project 

 You: Already set up! 
 
 Action: Just reload VSCode 
 Command: Ctrl+Shift+P "Developer: Reload Window" 
 
 Extension auto-activates for .nlpl files 
 
 Scenario 2: Using Extension in Other Projects (Your Machine) 

 Step 1: Run global install (one-time) 
 ./install_extension_globally.sh 
 
 Step 2: Configure new project (.vscode/settings.json) 
 { 
 "nexuslang.languageServer.path": 
 "/path/to/NexusLang/src/nxl_lsp.py" 
 } 
 
 Step 3: Open .nlpl files extension auto-activates! 
 
 Scenario 3: Sharing Extension with Others 

 Step 1: Build distributable package 
 ./package_extension.sh 
 Creates: nlpl-language-support-0.1.0.vsix 
 
 Step 2: Share .vsix file with users 
 - Email attachment 
 - GitHub Releases 
 - Internal file server 
 
 Step 3: Users install 
 code --install-extension nlpl-*.vsix 
 
 Scenario 4: Public Release (VSCode Marketplace) 

 Step 1: Create publisher account 
 https://marketplace.visualstudio.com/manage 
 
 Step 2: Install vsce CLI 
 npm install -g @vscode/vsce 
 
 Step 3: Login and publish 
 cd .vscode/extensions/nlpl 
 vsce login <publisher> 
 vsce publish 
 
 Step 4: Users install from marketplace 
 Search "NexusLang" in VSCode Extensions 
 Click "Install" 
 
```

---

## Feature Availability Matrix

```

 Feature VSCode Neovim Sublime Emacs 

 Syntax Highlighting 
 Real-time Diagnostics 
 Auto-completion 
 Signature Help 
 Code Actions 
 Go-to-Definition 
 Hover Information 
 Document Symbols 

Legend:
 = Fully implemented and tested
 = Configuration available (see DEPLOYMENT_GUIDE.md)
 = Not yet supported
```

---

## Quick Start Flowchart

```
 Want to use NLPL?
 
 Are you in the Is this a
 NexusLang repository? new project?
 
 YES YES 
 
 Just reload Run: 
 VSCode! ./install_ 
 Done! extension_ 
 globally.sh 
 
 Configure 
 project 
 Done! 
 
```

---

## Configuration Examples

### Minimal Configuration

**.vscode/settings.json (in new project):**
```json
{
 "nexuslang.languageServer.enabled": true
}
```
*LSP server auto-detected from global installation*

### Explicit Configuration

**.vscode/settings.json:**
```json
{
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "/absolute/path/to/NexusLang/src/nxl_lsp.py"
}
```
*Specify exact LSP server location*

### Debug Configuration

**.vscode/settings.json:**
```json
{
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "${workspaceFolder}/../NexusLang/src/nxl_lsp.py",
 "nexuslang.trace.server": "verbose"
}
```
*Relative paths and verbose logging*

### Multi-Root Workspace

**.code-workspace:**
```json
{
 "folders": [
 {"path": "project1"},
 {"path": "project2"}
 ],
 "settings": {
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "/home/user/NexusLang/src/nxl_lsp.py"
 }
}
```
*Shared settings across multiple folders*

---

## Troubleshooting Decision Tree

```
Extension not working?

 Is file .nlpl? NO Rename: file.nlpl
 YES

 Check language mode NOT NexusLang Select "NexusLang" (bottom-right)
 (bottom-right corner) NexusLang 

 Check Output panel NO LOGS LSP not starting
 View Output NexusLang Check Python version
 LOGS Check settings.json

 Python version? < 3.11 Upgrade Python
 python3 --version 3.11 

 Reload window? NO Ctrl+Shift+P Reload
 YES 

 Still broken? Report issue:
 github.com/Zajfan/NexusLang/issues
```

---

## Deployment Timeline

```
TODAY (Jan 5, 2026)

 Extension works in this repository
 Global installation script ready
 Package creation script ready
 Complete documentation

NEXT WEEK

 Create VSCode publisher account
 Publish to VSCode Marketplace
 Create GitHub Release with .vsix
 Test on Windows/Mac/Linux

NEXT MONTH

 Prepare PyPI package
 Create standalone binaries (PyInstaller)
 Setup documentation website
 Homebrew formula

NEXT QUARTER

 APT repository (Ubuntu/Debian)
 Chocolatey package (Windows)
 Docker Hub images
 JetBrains plugin (IntelliJ, PyCharm)

FUTURE

 Online playground/REPL
 Native compiler (LLVM)
 Package registry (like npm)
 Mobile development support
```

---

## Documentation Map

```
NLPL Documentation Structure

Root Level (Quick Access)
 QUICK_START.md ················· 5-minute tutorial
 DEPLOYMENT_GUIDE.md ············ Complete publishing guide
 DISTRIBUTION_SUMMARY.md ········ This file (overview)
 VSCODE_EXTENSION_GUIDE.md ······ Detailed feature guide
 README.md ·················· Project introduction
 ROADMAP.md ····················· Development roadmap

Installation Scripts
 install_extension_globally.sh ·· Install for all projects
 package_extension.sh ··········· Build distributable .vsix
 setup_vscode_extension.sh ······ Original setup (workspace-local)

Extension Files (.vscode/extensions/nlpl/)
 README.md ·················· Marketplace description
 CHANGELOG.md ··················· Version history
 LICENSE ···················· MIT license
 package.json ··················· Extension manifest
 extension.js ··················· Language client
 syntaxes/ ·················· Syntax highlighting

Detailed Documentation (docs/)
 1_introduction/ ················ Getting started
 2_language_basics/ ············· Syntax and grammar
 3_core_concepts/ ··············· OOP, error handling
 4_architecture/ ················ Compiler design
 5_type_system/ ················· Type system docs
 6_module_system/ ··············· Import/export
 7_development/ ················· Contributing guide

Examples (examples/)
 01-25_*.nlpl ··················· Tutorial programs
```

---

## Usage Commands Reference

### For This Repository (Already Set Up)

```bash
# Reload VSCode to activate extension
Ctrl+Shift+P "Developer: Reload Window"

# Open NexusLang file
code examples/01_basic_concepts.nlpl

# Run NexusLang program
python3 src/main.py examples/01_basic_concepts.nlpl
```

### For Other Projects on Your Machine

```bash
# One-time: Install globally
cd /path/to/NLPL
./install_extension_globally.sh

# For each new project:
cd /path/to/new-project
mkdir -p .vscode
cat > .vscode/settings.json << 'EOF'
{
 "nexuslang.languageServer.path": "/path/to/NexusLang/src/nxl_lsp.py"
}
EOF

# Reload VSCode
Ctrl+Shift+P "Developer: Reload Window"
```

### For Distribution to Others

```bash
# Build package
cd /path/to/NLPL
./package_extension.sh

# Output: nlpl-language-support-0.1.0.vsix

# Share file, users install with:
code --install-extension nlpl-language-support-0.1.0.vsix
```

### For VSCode Marketplace (Future)

```bash
# Install publishing tool
npm install -g @vscode/vsce

# Package and publish
cd .vscode/extensions/nlpl
vsce package
vsce publish

# Users search "NexusLang" in Extensions marketplace
```

---

## Features Demo

```
Open: examples/02_object_oriented.nlpl

Try These Features:

 1. Auto-completion 
 Type: func [Ctrl+Space] 
 Result: Shows "function", "function that takes", etc. 
 
 2. Signature Help 
 Type: call some_function with 
 Result: Shows parameter hints 
 
 3. Go-to-Definition 
 F12 on: any function name 
 Result: Jumps to function definition 
 
 4. Hover Information 
 Hover over: any variable or function 
 Result: Shows type and documentation 
 
 5. Quick Fixes 
 Create error: set x to "unclosed 
 Click: lightbulb icon 
 Result: Offers to fix unclosed string 
 
 6. Real-time Diagnostics 
 Type: set y to undefined_var 
 Result: Red squiggle + "Undefined variable" error 

```

---

## Summary

### What You Can Do Right Now

 **Use NexusLang extension in this project** - Just reload VSCode 
 **Install globally for all projects** - Run `./install_extension_globally.sh` 
 **Create distributable package** - Run `./package_extension.sh` 
 **Share with others** - Distribute .vsix file 
 **Configure other editors** - See DEPLOYMENT_GUIDE.md 

### What's Coming Next

 **VSCode Marketplace** - Public distribution 
 **PyPI Package** - `pip install nlpl` 
 **Binary Distributions** - Standalone executables 
 **Package Managers** - Homebrew, APT, Chocolatey 

### Getting Help

 **Guides:** QUICK_START.md, DEPLOYMENT_GUIDE.md 
 **Issues:** https://github.com/Zajfan/NexusLang/issues 
 **Docs:** `docs/` directory 
 **Examples:** `examples/` directory 

---

**NLPL is ready to use and deploy!** 
