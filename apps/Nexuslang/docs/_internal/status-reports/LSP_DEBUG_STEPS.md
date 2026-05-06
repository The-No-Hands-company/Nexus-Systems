# LSP Extension Debug Steps - Feb 17, 2026

## Problem: Extension stuck on "Activating..."

The extension is installed but hangs during activation. Here's how to debug it properly:

## Option 1: Run Extension in Development Mode (RECOMMENDED)

This shows all console output in real-time:

1. **Open the extension project in a NEW VS Code window:**
   ```bash
   code /run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NexusLang/vscode-extension
   ```

2. **Press F5** (or Run → Start Debugging)
   - This launches "Extension Development Host" - a new VS Code window with your extension loaded

3. **In the Extension Development Host window:**
   - Open the NexusLang workspace folder
   - Open a `.nlpl` file
   - Watch the DEBUG CONSOLE in the original window for `[NexusLang]` log messages

4. **Check the debug console for:**
   - `[NexusLang] Extension activation started`
   - `[NexusLang] Server command: python3`
   - `[NexusLang] Server args: ['-m', 'nexuslang.lsp', '--stdio']`
   - Any error messages

## Option 2: Check Extension Host Log

If development mode doesn't work:

1. **Help → Toggle Developer Tools** (Ctrl+Shift+I)
2. **Console tab**
3. Filter by "NexusLang" or "Extension Host"
4. Look for `[NexusLang]` prefixed messages or errors

## Option 3: Manual LSP Server Test

Test if the LSP server itself works:

```bash
cd /run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NLPL
PYTHONPATH=src python3 -m nexuslang.lsp --stdio
```

Then type this JSON-RPC initialize message:
```
Content-Length: 324

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":12345,"rootUri":"file:///path/to/workspace","capabilities":{},"initializationOptions":{},"workspaceFolders":[{"uri":"file:///path/to/workspace","name":"workspace"}]}}
```

Should respond with server capabilities. Press Ctrl+C to exit.

## Common Issues

### Issue: No output in console at all
- Extension not activating because .nlpl file not opened
- Extension disabled in settings
- VS Code needs reload

### Issue: "Cannot find module 'vscode-languageclient/node'"
- Dependencies not packaged correctly
- Need to check .vscodeignore includes `!node_modules/vscode-languageclient/**`

### Issue: Hangs on client.start()
- LSP server not starting (wrong Python path, wrong PYTHONPATH)
- LSP server crashes immediately (check stderr)
- LSP server doesn't respond to initialize (protocol issue)

## What We've Done So Far

1. ✅ Fixed .vscodeignore to include vscode-languageclient
2. ✅ Made activate() async and await client.start()
3. ✅ Added PYTHONPATH to server environment
4. ✅ Changed server launch to use `python3 -m nexuslang.lsp --stdio`
5. ✅ Added extensive console logging with [NexusLang] prefix
6. ⏳ Still stuck on "Activating..." - need to see logs

## Next Step

**Run in development mode (Option 1)** - this is the fastest way to see what's failing.
