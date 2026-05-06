"""
Signature Help Provider
========================

Provides parameter hints for function calls.
"""

from typing import Optional, Dict, List
import re


class SignatureHelpProvider:
    """
    Provides signature help for function calls.
    
    Shows:
    - Function parameters
    - Parameter types
    - Current parameter being typed
    - Documentation
    """
    
    def __init__(self, server):
        self.server = server
        
        # Standard library function signatures
        self.stdlib_signatures = {
            # Math module
            "sqrt": {
                "label": "sqrt with number as Float returns Float",
                "parameters": [
                    {"label": "number", "documentation": "The number to calculate square root of"}
                ],
                "documentation": "Calculate the square root of a number"
            },
            "sin": {
                "label": "sin with angle as Float returns Float",
                "parameters": [
                    {"label": "angle", "documentation": "Angle in radians"}
                ],
                "documentation": "Calculate sine of an angle"
            },
            "max": {
                "label": "max with a as Float, b as Float returns Float",
                "parameters": [
                    {"label": "a", "documentation": "First number"},
                    {"label": "b", "documentation": "Second number"}
                ],
                "documentation": "Return the maximum of two numbers"
            },
            
            # String module
            "split": {
                "label": "split with text as String, delimiter as String returns List of String",
                "parameters": [
                    {"label": "text", "documentation": "The string to split"},
                    {"label": "delimiter", "documentation": "The delimiter to split by"}
                ],
                "documentation": "Split a string by delimiter"
            },
            "join": {
                "label": "join with parts as List of String, separator as String returns String",
                "parameters": [
                    {"label": "parts", "documentation": "List of strings to join"},
                    {"label": "separator", "documentation": "String to use as separator"}
                ],
                "documentation": "Join a list of strings with a separator"
            },
            
            # IO module
            "print": {
                "label": "print text message as String",
                "parameters": [
                    {"label": "message", "documentation": "The message to print"}
                ],
                "documentation": "Print a message to stdout"
            },
            "read_file": {
                "label": "read_file with path as String returns String",
                "parameters": [
                    {"label": "path", "documentation": "Path to the file to read"}
                ],
                "documentation": "Read contents of a file"
            },
            "write_file": {
                "label": "write_file with path as String, content as String",
                "parameters": [
                    {"label": "path", "documentation": "Path to the file to write"},
                    {"label": "content", "documentation": "Content to write to file"}
                ],
                "documentation": "Write content to a file"
            },
        }
    
    def get_signature_help(self, text: str, position) -> Optional[Dict]:
        """
        Get signature help at position.
        
        Args:
            text: Document text
            position: Cursor position
            
        Returns:
            Signature help or None
        """
        lines = text.split('\n')
        if position.line >= len(lines):
            return None
        
        line = lines[position.line]
        prefix = line[:position.character]
        
        # Check if we're in a function call
        func_call = self._find_function_call(prefix)
        if not func_call:
            return None
        
        func_name = func_call['name']
        param_index = func_call['param_index']
        
        # Try to find signature
        signature = self._get_signature(text, func_name)
        if not signature:
            return None
        
        return {
            "signatures": [signature],
            "activeSignature": 0,
            "activeParameter": param_index
        }
    
    def _find_function_call(self, prefix: str) -> Optional[Dict]:
        """
        Find function call context in prefix.
        
        Returns:
            Dict with 'name' and 'param_index' or None
        """
        # Look for patterns like: function_name with arg1, arg2
        # or: function_name that takes arg1, arg2
        
        # Pattern 1: "func_name with ..."
        match = re.search(r'(\w+)\s+with\s+([^)]*?)$', prefix, re.IGNORECASE)
        if match:
            func_name = match.group(1)
            args_part = match.group(2)
            param_index = args_part.count(',')
            return {"name": func_name, "param_index": param_index}
        
        # Pattern 2: Inside function definition "that takes ..."
        match = re.search(r'function\s+(\w+)\s+that\s+takes\s+([^)]*?)$', prefix, re.IGNORECASE)
        if match:
            func_name = match.group(1)
            params_part = match.group(2)
            param_index = params_part.count(',')
            return {"name": func_name, "param_index": param_index}
        
        return None
    
    def _get_signature(self, text: str, func_name: str) -> Optional[Dict]:
        """Get signature for function."""
        # Check stdlib signatures
        if func_name in self.stdlib_signatures:
            sig = self.stdlib_signatures[func_name]
            return {
                "label": sig["label"],
                "documentation": sig["documentation"],
                "parameters": sig["parameters"]
            }
        
        # Try to find function definition in document
        signature = self._extract_function_signature(text, func_name)
        if signature:
            return signature
        
        return None
    
    def _extract_function_signature(self, text: str, func_name: str) -> Optional[Dict]:
        """Extract function signature from document."""
        lines = text.split('\n')
        
        # Look for function definition
        pattern = rf'function\s+{re.escape(func_name)}\s+(.*?)(?:\n|$)'
        
        for line in lines:
            match = re.search(pattern, line, re.IGNORECASE)
            if match:
                signature_text = match.group(0).strip()
                
                # Parse parameters
                params = []
                params_match = re.search(r'that\s+takes\s+(.*?)\s+returns', signature_text, re.IGNORECASE)
                if params_match:
                    params_text = params_match.group(1)
                    # Split by commas and parse each parameter
                    for param_part in params_text.split(','):
                        param_part = param_part.strip()
                        # Format: "param_name as Type"
                        param_match = re.match(r'(\w+)(?:\s+as\s+(\w+))?', param_part, re.IGNORECASE)
                        if param_match:
                            param_name = param_match.group(1)
                            param_type = param_match.group(2) if param_match.group(2) else "Any"
                            params.append({
                                "label": f"{param_name} as {param_type}",
                                "documentation": f"Parameter {param_name}"
                            })
                
                # Extract return type
                returns_match = re.search(r'returns\s+(\w+)', signature_text, re.IGNORECASE)
                return_type = returns_match.group(1) if returns_match else "Any"
                
                return {
                    "label": signature_text,
                    "documentation": f"Function {func_name}",
                    "parameters": params
                }
        
        return None


__all__ = ['SignatureHelpProvider']
