#!/usr/bin/env python3
"""
Interactive LSP Server Test
============================

This script helps you manually test the NexusLang LSP server by:
1. Starting the server
2. Sending sample LSP requests
3. Showing responses in readable format

Usage:
    python3 dev_tools/test_lsp_interactive.py
"""

import subprocess
import json
import sys
import time

def send_lsp_request(process, request_id, method, params=None):
    """Send an LSP request and return the response."""
    request = {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": method
    }
    if params:
        request["params"] = params
    
    # Convert to JSON and add Content-Length header
    request_json = json.dumps(request)
    message = f"Content-Length: {len(request_json)}\r\n\r\n{request_json}"
    
    print(f"\n{'='*60}")
    print(f"SENDING: {method} (ID: {request_id})")
    print(f"{'='*60}")
    print(json.dumps(request, indent=2))
    
    # Send to server
    process.stdin.write(message)
    process.stdin.flush()
    
    # Read response
    time.sleep(0.5)  # Give server time to process
    
    return True


def test_lsp_server():
    """Run interactive LSP server tests."""
    print("=" * 60)
    print("NLPL LSP Server Interactive Test")
    print("=" * 60)
    print()
    
    # Start LSP server
    print("Starting LSP server...")
    server = subprocess.Popen(
        ["python3", "src/nlpl/lsp/server.py"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )
    
    print("Server started (PID: {})".format(server.pid))
    time.sleep(0.5)
    
    try:
        # Test 1: Initialize
        print("\n" + "="*60)
        print("TEST 1: Initialize Server")
        print("="*60)
        
        init_params = {
            "processId": None,
            "rootUri": "file:///run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NLPL",
            "capabilities": {
                "textDocument": {
                    "completion": {"dynamicRegistration": True},
                    "hover": {"dynamicRegistration": True}
                }
            }
        }
        
        send_lsp_request(server, 1, "initialize", init_params)
        
        # Read response
        print("\nWaiting for response...")
        time.sleep(2)
        
        # Try to read output
        output_lines = []
        try:
            while True:
                line = server.stdout.readline()
                if not line:
                    break
                output_lines.append(line)
                if len(output_lines) > 50:  # Limit output
                    break
        except:
            pass
        
        if output_lines:
            print("\nSERVER RESPONSE:")
            print("-" * 60)
            for line in output_lines[:30]:  # Show first 30 lines
                print(line.rstrip())
            if len(output_lines) > 30:
                print(f"... ({len(output_lines) - 30} more lines)")
        else:
            print("\nNo response received (server may be waiting for more input)")
        
        # Test 2: Initialized notification
        print("\n" + "="*60)
        print("TEST 2: Initialized Notification")
        print("="*60)
        
        initialized = {
            "jsonrpc": "2.0",
            "method": "initialized",
            "params": {}
        }
        initialized_json = json.dumps(initialized)
        message = f"Content-Length: {len(initialized_json)}\r\n\r\n{initialized_json}"
        server.stdin.write(message)
        server.stdin.flush()
        
        print("Sent initialized notification")
        time.sleep(1)
        
        # Test 3: Shutdown
        print("\n" + "="*60)
        print("TEST 3: Shutdown Server")
        print("="*60)
        
        send_lsp_request(server, 2, "shutdown")
        time.sleep(1)
        
        # Exit notification
        exit_notif = {
            "jsonrpc": "2.0",
            "method": "exit"
        }
        exit_json = json.dumps(exit_notif)
        message = f"Content-Length: {len(exit_json)}\r\n\r\n{exit_json}"
        server.stdin.write(message)
        server.stdin.flush()
        
        print("Sent exit notification")
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
    except Exception as e:
        print(f"\n\nError during test: {e}")
    finally:
        # Clean up
        try:
            server.terminate()
            server.wait(timeout=2)
        except:
            server.kill()
        
        print("\n" + "="*60)
        print("Test Complete")
        print("="*60)


def simple_test():
    """Even simpler test - just check if server responds."""
    print("=" * 60)
    print("SIMPLE LSP SERVER TEST")
    print("=" * 60)
    print()
    print("Starting server and sending initialize request...")
    print()
    
    # Create initialize request
    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "processId": None,
            "rootUri": "file:///run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NLPL",
            "capabilities": {}
        }
    }
    
    request_json = json.dumps(request)
    message = f"Content-Length: {len(request_json)}\r\n\r\n{request_json}"
    
    # Run server with input
    result = subprocess.run(
        ["python3", "src/nlpl/lsp/server.py"],
        input=message,
        capture_output=True,
        text=True,
        timeout=5
    )
    
    print("STDOUT:")
    print("-" * 60)
    print(result.stdout[:2000])  # First 2000 chars
    
    if len(result.stdout) > 2000:
        print(f"\n... (output truncated, {len(result.stdout)} total chars)")
    
    print("\nSTDERR:")
    print("-" * 60)
    print(result.stderr[:1000] if result.stderr else "(empty)")
    
    print("\nReturn code:", result.returncode)
    
    # Try to find JSON response
    if "capabilities" in result.stdout:
        print("\n✅ Server responded with capabilities!")
        print("\nLooking for key capabilities:")
        if "definitionProvider" in result.stdout:
            print("  ✅ definitionProvider")
        if "referencesProvider" in result.stdout:
            print("  ✅ referencesProvider")
        if "documentSymbolProvider" in result.stdout:
            print("  ✅ documentSymbolProvider")
        if "callHierarchyProvider" in result.stdout:
            print("  ✅ callHierarchyProvider")
    else:
        print("\n⚠️ No capabilities found in response")


if __name__ == "__main__":
    print("\nChoose test mode:")
    print("1. Simple test (recommended)")
    print("2. Interactive test (advanced)")
    print()
    
    choice = input("Enter choice (1 or 2, default=1): ").strip() or "1"
    print()
    
    if choice == "2":
        test_lsp_server()
    else:
        simple_test()
