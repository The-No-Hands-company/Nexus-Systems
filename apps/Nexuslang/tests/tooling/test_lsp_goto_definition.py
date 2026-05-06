#!/usr/bin/env python3
"""
LSP Go-To-Definition Test
==========================

Tests go-to-definition functionality including cross-file navigation.
"""

import json
import subprocess
import sys
import time
from pathlib import Path

# Add NexusLang to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'src'))


class LSPTestClient:
    """Simple LSP client for testing."""
    
    def __init__(self):
        self.process = None
        self.request_id = 0
        
    def start(self):
        """Start the LSP server."""
        import os
        env = os.environ.copy()
        # Use absolute resolved path — conftest only sets sys.path in-process,
        # not os.environ, so the subprocess needs PYTHONPATH set explicitly.
        src_dir = str(Path(__file__).parent.parent.parent.resolve() / "src")
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = src_dir + (os.pathsep + existing if existing else "")
        self.process = subprocess.Popen(
            [sys.executable, '-m', 'nexuslang.lsp', '--stdio'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=Path(__file__).parent.parent.parent,
            env=env,
        )
        time.sleep(0.5)
        
    def send_message(self, message):
        """Send a JSON-RPC message."""
        content = json.dumps(message)
        header = f"Content-Length: {len(content)}\r\n\r\n"
        full_message = header + content
        self.process.stdin.write(full_message.encode('utf-8'))
        self.process.stdin.flush()
        
    def read_message(self):
        """Read the next JSON-RPC message (may be a notification or a response)."""
        headers = {}
        while True:
            line = self.process.stdout.readline().decode('utf-8')
            if line in ('\r\n', '\n'):
                break
            if ':' in line:
                key, value = line.split(':', 1)
                headers[key.strip()] = value.strip()

        content_length = int(headers.get('Content-Length', 0))
        content = self.process.stdout.read(content_length).decode('utf-8')
        return json.loads(content)

    def read_response(self):
        """Read messages until a JSON-RPC response is found (skips notifications)."""
        while True:
            msg = self.read_message()
            # Responses have an 'id' field; notifications only have 'method'
            if 'id' in msg:
                return msg
        
    def initialize(self, root_path):
        """Initialize the LSP server."""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": "initialize",
            "params": {
                "processId": self.process.pid,
                "rootUri": f"file://{root_path}",
                "capabilities": {
                    "textDocument": {
                        "definition": {"linkSupport": True}
                    }
                }
            }
        }
        self.send_message(request)
        return self.read_response()
        
    def initialized(self):
        """Send initialized notification."""
        self.send_message({
            "jsonrpc": "2.0",
            "method": "initialized",
            "params": {}
        })
        time.sleep(0.2)
        
    def open_document(self, uri, content):
        """Open a document."""
        self.send_message({
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri,
                    "languageId": "nlpl",
                    "version": 1,
                    "text": content
                }
            }
        })
        time.sleep(0.1)
        
    def goto_definition(self, uri, line, character):
        """Request go-to-definition."""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": "textDocument/definition",
            "params": {
                "textDocument": {"uri": uri},
                "position": {"line": line, "character": character}
            }
        }
        self.send_message(request)
        return self.read_response()
        
    def shutdown(self):
        """Shutdown the server."""
        self.request_id += 1
        self.send_message({
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": "shutdown",
            "params": None
        })
        try:
            self.read_response()
        except:
            pass
        self.send_message({
            "jsonrpc": "2.0",
            "method": "exit",
            "params": None
        })
        self.process.wait(timeout=2)


def test_goto_definition_same_file():
    """Test go-to-definition within the same file."""
    print("\n" + "=" * 70)
    print("TEST: Go-To-Definition (Same File)")
    print("=" * 70)
    
    # Test NexusLang code
    test_code = """function calculate_sum with a as Integer, b as Integer returns Integer
    return a plus b
end

set result to calculate_sum with 5 and 10
print text result
"""
    
    client = LSPTestClient()
    client.start()
    
    test_file = Path(__file__).parent.parent.parent / "test_programs" / "lsp_tests" / "test_same_file.nxl"
    test_file.parent.mkdir(parents=True, exist_ok=True)
    test_file.write_text(test_code)
    
    uri = f"file://{test_file}"
    
    # Initialize
    client.initialize(test_file.parent)
    client.initialized()
    
    # Open document
    client.open_document(uri, test_code)
    
    # Test 1: Go to function definition from function call
    # Line 4 (0-indexed = 3), character 14 (inside "calculate_sum")
    print("\n1. Testing: Go to function definition from call site...")
    response = client.goto_definition(uri, 4, 14)
    
    if 'result' in response and response['result']:
        location = response['result']
        if isinstance(location, list):
            location = location[0]
        print(f"   ✅ Found definition at line {location['range']['start']['line']}")
        if location['range']['start']['line'] == 0:  # Function defined at line 0
            print("   ✅ Correct line!")
        else:
            print(f"   ❌ Wrong line (expected 0, got {location['range']['start']['line']})")
    else:
        print(f"   ❌ No definition found: {response}")
        
    # Test 2: Go to parameter definition
    print("\n2. Testing: Go to parameter definition...")
    # Line 1, character 11 (inside "a plus b")
    response = client.goto_definition(uri, 1, 11)
    
    if 'result' in response and response['result']:
        location = response['result']
        if isinstance(location, list):
            location = location[0]
        print(f"   ✅ Found definition at line {location['range']['start']['line']}")
    else:
        print(f"   ❌ No definition found: {response}")
    
    client.shutdown()
    print("\n" + "=" * 70)


def test_goto_definition_cross_file():
    """Test go-to-definition across files (imports)."""
    print("\n" + "=" * 70)
    print("TEST: Go-To-Definition (Cross-File)")
    print("=" * 70)
    
    test_dir = Path(__file__).parent.parent.parent / "test_programs" / "lsp_tests"
    
    # Read the test files
    module_a_path = test_dir / "test_module_a.nxl"
    module_b_path = test_dir / "test_module_b.nxl"
    
    if not module_a_path.exists() or not module_b_path.exists():
        print("Test files not found!")
        import pytest; pytest.skip("LSP fixture files test_module_a/b.nlpl not found")
        
    module_a_content = module_a_path.read_text()
    module_b_content = module_b_path.read_text()
    
    client = LSPTestClient()
    client.start()
    
    # Initialize
    client.initialize(test_dir)
    client.initialized()
    
    # Open both documents
    uri_a = f"file://{module_a_path}"
    uri_b = f"file://{module_b_path}"
    
    client.open_document(uri_a, module_a_content)
    client.open_document(uri_b, module_b_content)
    
    # Test 1: Go to function definition from module B
    # module_b line 6 (0-indexed): "set message to greet with 'World'"
    # 'greet' starts at character 15
    print("\n1. Testing: Go to imported function definition...")
    response = client.goto_definition(uri_b, 6, 15)
    
    if 'result' in response and response['result']:
        location = response['result']
        if isinstance(location, list):
            location = location[0]
        print(f"   ✅ Found definition")
        print(f"   File: {location['uri']}")
        print(f"   Line: {location['range']['start']['line']}")
        
        # Check if it points to module_a
        if 'test_module_a.nxl' in location['uri']:
            print("   ✅ Correct file (test_module_a.nxl)!")
        else:
            print(f"   ❌ Wrong file (expected test_module_a.nxl)")
            
        if location['range']['start']['line'] == 3:  # greet function at line 3
            print("   ✅ Correct line!")
        else:
            print(f"   ⚠️  Line mismatch (expected 3, got {location['range']['start']['line']})")
    else:
        print(f"   ❌ No definition found: {response}")
        print("   This is a KNOWN ISSUE - cross-file go-to-definition needs work!")
        
    # Test 2: Go to class definition
    print("\n2. Testing: Go to imported class definition...")
    # module_b line 10 (0-indexed): "set user to new User"
    # 'User' starts at character 16
    response = client.goto_definition(uri_b, 10, 16)
    
    if 'result' in response and response['result']:
        location = response['result']
        if isinstance(location, list):
            location = location[0]
        print(f"   ✅ Found definition")
        print(f"   File: {location['uri']}")
        print(f"   Line: {location['range']['start']['line']}")
        
        if 'test_module_a.nxl' in location['uri']:
            print("   ✅ Correct file!")
        else:
            print(f"   ❌ Wrong file")
    else:
        print(f"   ❌ No definition found: {response}")
        print("   This is a KNOWN ISSUE - cross-file go-to-definition needs work!")
    
    client.shutdown()
    print("\n" + "=" * 70)


if __name__ == '__main__':
    try:
        print("\n" + "=" * 70)
        print("LSP GO-TO-DEFINITION COMPREHENSIVE TEST")
        print("=" * 70)
        
        # Test same-file navigation
        test_goto_definition_same_file()
        
        # Test cross-file navigation
        test_goto_definition_cross_file()
        
        print("\n" + "=" * 70)
        print("ALL TESTS COMPLETED")
        print("=" * 70)
        print("\nNote: Cross-file navigation may not work yet - this is expected.")
        print("This test helps identify what needs to be fixed.")
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
