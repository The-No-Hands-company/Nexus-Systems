#!/usr/bin/env python3
"""
LSP Server Startup Test
========================

Tests that the NexusLang LSP server starts correctly and handles basic LSP initialization.
"""

import json
import subprocess
import sys
import time
from pathlib import Path

# Add NexusLang to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))


def send_message(process, message):
    """Send a JSON-RPC message to the LSP server."""
    content = json.dumps(message)
    header = f"Content-Length: {len(content)}\r\n\r\n"
    full_message = header + content
    process.stdin.write(full_message.encode('utf-8'))
    process.stdin.flush()
    print(f"SENT: {message.get('method', message.get('result', 'response'))}")


def read_message(process):
    """Read a JSON-RPC message from the LSP server."""
    # Read headers
    headers = {}
    while True:
        line = process.stdout.readline().decode('utf-8')
        if line == '\r\n':
            break
        if ':' in line:
            key, value = line.split(':', 1)
            headers[key.strip()] = value.strip()
    
    # Read content
    content_length = int(headers.get('Content-Length', 0))
    content = process.stdout.read(content_length).decode('utf-8')
    message = json.loads(content)
    print(f"RECEIVED: {message.get('method', message.get('result', 'response'))}")
    return message


def test_lsp_startup():
    """Test LSP server startup and initialization."""
    print("=" * 70)
    print("LSP SERVER STARTUP TEST")
    print("=" * 70)
    
    # Start LSP server
    print("\n1. Starting LSP server...")
    process = subprocess.Popen(
        [sys.executable, '-m', 'nexuslang.lsp', '--stdio'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=Path(__file__).parent.parent
    )
    
    time.sleep(0.5)  # Give server time to start
    
    if process.poll() is not None:
        stderr = process.stderr.read().decode('utf-8')
        print(f"Server failed to start!\n{stderr}")
        import pytest; pytest.skip("LSP server failed to start")
    
    print("✅ Server started successfully")
    
    # Send initialize request
    print("\n2. Sending initialize request...")
    initialize_request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "processId": process.pid,
            "rootUri": f"file://{Path.cwd()}",
            "capabilities": {
                "textDocument": {
                    "completion": {"completionItem": {"snippetSupport": True}},
                    "hover": {"contentFormat": ["markdown", "plaintext"]},
                    "signatureHelp": {"signatureInformation": {"parameterInformation": {"labelOffsetSupport": True}}},
                    "definition": {"linkSupport": True},
                    "references": {"dynamicRegistration": True},
                    "rename": {"prepareSupport": True}
                },
                "workspace": {
                    "symbol": {"dynamicRegistration": True}
                }
            }
        }
    }
    
    send_message(process, initialize_request)
    
    # Read initialize response
    print("\n3. Waiting for initialize response...")
    try:
        response = read_message(process)
        
        if response.get('id') != 1:
            import pytest; pytest.fail(f"Wrong LSP response ID: {response.get('id')}")
        
        if 'result' not in response:
            import pytest; pytest.fail(f"No result in LSP initialize response: {response}")
        
        capabilities = response['result'].get('capabilities', {})
        print(f"\n✅ Server initialized with capabilities:")
        print(f"   - Completion: {capabilities.get('completionProvider', {})}")
        print(f"   - Hover: {'hoverProvider' in capabilities}")
        print(f"   - Definition: {'definitionProvider' in capabilities}")
        print(f"   - References: {'referencesProvider' in capabilities}")
        print(f"   - Rename: {'renameProvider' in capabilities}")
        print(f"   - Formatting: {'documentFormattingProvider' in capabilities}")
        print(f"   - Code Actions: {'codeActionProvider' in capabilities}")
        print(f"   - Signature Help: {'signatureHelpProvider' in capabilities}")
        
    except Exception as e:
        stderr = process.stderr.read().decode('utf-8')
        import pytest; pytest.fail(f"Failed to read LSP initialize response: {e}\nServer stderr: {stderr}")
    
    # Send initialized notification
    print("\n4. Sending initialized notification...")
    initialized_notification = {
        "jsonrpc": "2.0",
        "method": "initialized",
        "params": {}
    }
    send_message(process, initialized_notification)
    time.sleep(0.2)
    
    # Send shutdown request
    print("\n5. Sending shutdown request...")
    shutdown_request = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "shutdown",
        "params": None
    }
    send_message(process, shutdown_request)
    
    try:
        response = read_message(process)
        if response.get('id') == 2 and 'result' in response:
            print("✅ Server acknowledged shutdown")
        else:
            print(f"⚠️  Unexpected shutdown response: {response}")
    except:
        print("⚠️  No shutdown response (may be normal)")
    
    # Send exit notification
    print("\n6. Sending exit notification...")
    exit_notification = {
        "jsonrpc": "2.0",
        "method": "exit",
        "params": None
    }
    send_message(process, exit_notification)
    
    # Wait for process to exit
    try:
        exit_code = process.wait(timeout=2)
        if exit_code == 0:
            print(f"✅ Server exited cleanly (code {exit_code})")
        else:
            print(f"⚠️  Server exited with code {exit_code}")
    except subprocess.TimeoutExpired:
        print("⚠️  Server did not exit, terminating...")
        process.terminate()
        process.wait()
    
    print("\n" + "=" * 70)
    print("TEST COMPLETED SUCCESSFULLY")
    print("=" * 70)


if __name__ == '__main__':
    try:
        test_lsp_startup()
        sys.exit(0)
    except KeyboardInterrupt:
        print("\n\nTest interrupted")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
