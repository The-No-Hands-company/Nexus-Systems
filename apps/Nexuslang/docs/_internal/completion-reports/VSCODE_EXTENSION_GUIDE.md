# Using NexusLang Extension in This Project

## Extension is Already Set Up!

The NexusLang VSCode extension is configured and ready to use in this workspace. No need to start a new project!

## What's Configured

- **Extension location**: `.vscode/extensions/nlpl/`
- **Settings file**: `.vscode/settings.json` (auto-configured)
- **LSP server path**: `${workspaceFolder}/src/nxl_lsp.py`

## How to Use

### Option 1: Reload VSCode (Easiest)

1. **Reload the window**:
 - Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
 - Type: `Developer: Reload Window`
 - Press Enter

2. **Open any NexusLang file**:
 ```bash
 code examples/01_basic_concepts.nlpl
 ```
 or
 ```bash
 code test_programs/lsp_test.nlpl
 ```

3. **The extension should activate automatically!** 

You'll see:
- Syntax highlighting for `.nlpl` files
- Real-time error checking as you type
- Auto-completion (press `Ctrl+Space`)
- Signature help (parameter hints)
- Code actions (quick fixes)
- Go-to-definition (`F12` or `Ctrl+Click`)
- Hover documentation

### Option 2: Developer Mode (For Testing)

If Option 1 doesn't work, you can run the extension in development mode:

1. **Open the extension folder**:
 ```bash
 code .vscode/extensions/nlpl
 ```

2. **Press F5** to launch Extension Development Host

3. **In the new window**, open your NexusLang workspace

### Option 3: Install Extension Globally

To use the extension in ANY VSCode window (not just this project):

```bash
cd .vscode/nlpl-extension
npm install
npm run compile
npm install -g @vscode/vsce
vsce package
code --install-extension nlpl-language-support-0.1.0.vsix
```

## Test It's Working

### 1. Check Language Mode

Open a `.nlpl` file and look at the bottom-right corner of VSCode. It should say **"NexusLang"**.

If not, click the language indicator and select "NexusLang" from the list.

### 2. Test Diagnostics

Create a test file with an error:

```nlpl
set x to "unclosed string
```

You should see:
- Red squiggly line under the error
- Error in the Problems panel (`Ctrl+Shift+M`)

### 3. Test Completions

Type in a `.nlpl` file:
```nlpl
set result to sq
```

Press `Ctrl+Space` and you should see completion suggestions like `sqrt`.

### 4. Test Signature Help

Type:
```nlpl
set result to sqrt with 
```

You should see a popup showing: `sqrt with number as Float returns Float`

### 5. Test Code Actions

Type:
```nlpl
set unused_var to 42
```

You should see:
- Warning about unused variable
- Light bulb icon (or press `Ctrl+.`)
- Quick fix: "Remove unused variable"

## Troubleshooting

### Extension Not Activating

1. **Check if NexusLang file type is recognized**:
 - Open Command Palette (`Ctrl+Shift+P`)
 - Type: `Change Language Mode`
 - Look for "NexusLang" in the list

2. **Check extension logs**:
 - View Output
 - Select "NLPL Language Server" from dropdown
 - Look for activation messages

3. **Verify Python is available**:
 ```bash
 python3 --version
 which python3
 ```

4. **Test LSP server manually**:
 ```bash
 python3 src/nxl_lsp.py
 ```
 (Should wait for input, press `Ctrl+C` to exit)

### No Diagnostics Appearing

1. **Enable verbose logging**:
 - Open Settings (`Ctrl+,`)
 - Search: `nlpl.trace.server`
 - Set to: `verbose`

2. **Check server is running**:
 - View Output "NLPL Language Server"
 - Should see: "NLPL Language Server started"

3. **Verify file has `.nlpl` extension**

### Syntax Highlighting Not Working

1. **Reload grammar**:
 - Command Palette `Developer: Force Retokenize`

2. **Check file association**:
 - Settings `files.associations`
 - Should have: `"*.nxl": "nlpl"`

## Check Extension Status

Run this to see if everything is in place:

```bash
./setup_vscode_extension.sh
```

Expected output:
```
 Node.js found
 npm found
 Python 3 found
Extension configured in: .vscode/settings.json
```

## Quick Test Suite

Test all LSP features:

```bash
# Test diagnostics
python dev_tools/test_lsp_diagnostics.py

# Test enhanced features
python dev_tools/test_lsp_enhanced.py
```

## Pro Tips

1. **Quick Commands**:
 - `F12` - Go to definition
 - `Ctrl+Space` - Trigger completions
 - `Ctrl+Shift+Space` - Signature help
 - `Ctrl+.` - Quick fixes
 - `Shift+F12` - Find all references

2. **Customize Settings**:
 Edit `.vscode/settings.json`:
 ```json
 {
 "nexuslang.languageServer.enabled": true,
 "nexuslang.trace.server": "verbose",
 "editor.quickSuggestions": {
 "other": true,
 "strings": false
 }
 }
 ```

3. **See Diagnostics Panel**:
 - Press `Ctrl+Shift+M`
 - Or View Problems

## You're Ready!

Just **reload VSCode** and open any `.nlpl` file. The extension will activate automatically with full LSP support!

---

**Need Help?** Check the logs in View Output "NLPL Language Server"
