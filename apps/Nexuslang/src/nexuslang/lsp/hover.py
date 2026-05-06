"""
Hover Provider
==============

Provides documentation and type information on hover.
"""

from typing import Optional, Dict
import re


class HoverProvider:
    """
    Provides hover information for symbols.
    
    Shows:
    - Function signatures
    - Variable types
    - Documentation
    - Examples
    """
    
    def __init__(self, server):
        self.server = server
        
        # Documentation for keywords and stdlib
        self.docs = {
            # Keywords
            "function": "Define a function\n\nSyntax:\n```nlpl\nfunction name that takes param as Type returns Type\n    # body\n```",
            "class": "Define a class\n\nSyntax:\n```nlpl\nclass Name\n    property field as Type\n```",
            "set": "Declare or assign a variable\n\nSyntax:\n```nlpl\nset varname to value\n```",
            "if": "Conditional statement\n\nSyntax:\n```nlpl\nif condition\n    # body\n```",
            "for": "For-each loop\n\nSyntax:\n```nlpl\nfor each item in collection\n    # body\n```",
            "while": "While loop\n\nSyntax:\n```nlpl\nwhile condition\n    # body\n```",
            "return": "Return value from function\n\nSyntax:\n```nlpl\nreturn value\n```",
            "yield": "Yield a value from a generator function.\n\nSyntax:\n```nlpl\nyield value\n```\n\nNotes:\n- `yield` marks the function as a generator\n- Generator signatures are represented as `List<T>` where `T` is the yielded element type",
            "generator": "Generator functions produce a sequence of values over time using `yield`.\n\nExample:\n```nlpl\nfunction count_up_to that takes n as Integer returns List of Integer\n    set i to 0\n    while i is less than n\n        yield i\n        set i to i plus 1\n    end\nend\n```",
            "import": "Import module\n\nSyntax:\n```nlpl\nimport module_name\nfrom module_name import symbol\n```",
            "channel": "Channel primitive for concurrent message passing.\n\nSyntax:\n```nlpl\nset ch to create channel\nsend 42 to ch\nset value to receive from ch\n```",
            "Channel": "Typed channel for message passing between concurrent tasks.\n\nExamples:\n```nlpl\nset ch as Channel<Integer> to create channel\nsend 1 to ch\nset x to receive from ch\n```",
            "create": "Create runtime values such as channels and collections.\n\nChannel example:\n```nlpl\nset ch to create channel\n```",
            "send": "Send a value to a channel.\n\nSyntax:\n```nlpl\nsend value to channel_var\n```",
            "receive": "Receive a value from a channel.\n\nSyntax:\n```nlpl\nset value to receive from channel_var\n```",
            "macro": "Define a macro for reusable compile-time expansion.\n\nSyntax:\n```nlpl\nmacro NAME with arg1, arg2\n    # body\nend\n```",
            "expand": "Expand a macro invocation in the current scope.\n\nSyntax:\n```nlpl\nexpand NAME\nexpand NAME with arg1 value1\n```",
            "comptime": "Compile-time execution surface.\n\nForms:\n```nlpl\ncomptime eval EXPR\ncomptime const NAME is EXPR\ncomptime assert COND\ncomptime assert COND message \"reason\"\n```",
            "eval": "Evaluate expression during compile time.\n\nSyntax:\n```nlpl\ncomptime eval EXPR\n```",
            "const": "Define immutable compile-time constant.\n\nSyntax:\n```nlpl\ncomptime const NAME is EXPR\n```",
            "asm": "Inline assembly block with explicit operand constraints.\n\nSyntax:\n```nlpl\nasm\n    code\n        \"nop\"\n    inputs \"r\": value\n    outputs \"=r\": out_var\n    clobbers \"memory\", \"cc\"\nend\n```",
            "inputs": "Inline-assembly input operands.\n\nUse GCC-style constraint tokens like `r`, `m`, `i`.\n\nExample:\n```nlpl\ninputs \"r\": src\n```",
            "outputs": "Inline-assembly output operands.\n\nOutput constraints should include write markers (`=` or `+`).\n\nExample:\n```nlpl\noutputs \"=r\": result\n```",
            "clobbers": "Inline-assembly clobber list.\n\nUse `memory`, `cc`, or register names (for example `rax`).\n\nExample:\n```nlpl\nclobbers \"memory\", \"cc\", \"rax\"\n```",
            "constraint": "Inline-assembly operand constraint token.\n\nCommon forms: `r`, `m`, `i` for inputs and `=r`, `+m` for outputs.",
            "unsafe": "Explicit unsafe execution region for low-level operations.\n\nSyntax:\n```nlpl\nunsafe do\n    # FFI calls, raw pointers, manual memory operations\nend\n```",
            "extern": "Declare external symbols for FFI binding.\n\nFunction syntax:\n```nlpl\nextern function puts with text as Pointer returns Integer from library \"c\"\n```\n\nVariable syntax:\n```nlpl\nextern variable errno as Integer from library \"c\"\n```",
            "foreign": "Alias for `extern` declarations in FFI contexts.\n\nExample:\n```nlpl\nforeign function malloc with size as Integer returns Pointer from library \"c\"\n```",
            "calling": "Calling convention clause used in extern function declarations.\n\nSyntax:\n```nlpl\nextern function fn with x as Integer returns Integer from library \"mylib\" calling convention cdecl\n```",
            "convention": "Calling convention selector for extern functions.\n\nCommon values: `cdecl`, `stdcall`, `fastcall`, `sysv`, `win64`, `aapcs`.",
            
            # Math module
            "sqrt": "**sqrt** - Square root\n\n**From**: math\n\n**Syntax**: `sqrt with number`\n\n**Returns**: Float\n\n**Example**:\n```nlpl\nset result to sqrt with 16  # 4.0\n```",
            "sin": "**sin** - Sine function\n\n**From**: math\n\n**Syntax**: `sin with angle`\n\n**Returns**: Float\n\n**Example**:\n```nlpl\nset result to sin with 0  # 0.0\n```",
            "cos": "**cos** - Cosine function\n\n**From**: math\n\n**Syntax**: `cos with angle`\n\n**Returns**: Float",
            "tan": "**tan** - Tangent function\n\n**From**: math\n\n**Syntax**: `tan with angle`\n\n**Returns**: Float",
            "floor": "**floor** - Round down\n\n**From**: math\n\n**Syntax**: `floor with number`\n\n**Returns**: Integer",
            "ceil": "**ceil** - Round up\n\n**From**: math\n\n**Syntax**: `ceil with number`\n\n**Returns**: Integer",
            "abs": "**abs** - Absolute value\n\n**From**: math\n\n**Syntax**: `abs with number`\n\n**Returns**: Float",
            "max": "**max** - Maximum value\n\n**From**: math\n\n**Syntax**: `max with numbers`\n\n**Returns**: Float",
            "min": "**min** - Minimum value\n\n**From**: math\n\n**Syntax**: `min with numbers`\n\n**Returns**: Float",
            
            # String module
            "split": "**split** - Split string by delimiter\n\n**From**: string\n\n**Syntax**: `split with text, delimiter`\n\n**Returns**: List of String\n\n**Example**:\n```nlpl\nset parts to split with \"a,b,c\", \",\"\n```",
            "join": "**join** - Join strings with separator\n\n**From**: string\n\n**Syntax**: `join with separator, strings`\n\n**Returns**: String",
            "trim": "**trim** - Remove whitespace\n\n**From**: string\n\n**Syntax**: `trim with text`\n\n**Returns**: String",
            "replace": "**replace** - Replace substring\n\n**From**: string\n\n**Syntax**: `replace with text, old, new`\n\n**Returns**: String",
            "substring": "**substring** - Extract substring\n\n**From**: string\n\n**Syntax**: `substring with text, start, end`\n\n**Returns**: String",
            "length": "**length** - String length\n\n**From**: string\n\n**Syntax**: `length with text`\n\n**Returns**: Integer",
            
            # I/O module
            "print": "**print** - Print to stdout\n\n**From**: io\n\n**Syntax**: `print text message`\n\n**Example**:\n```nlpl\nprint text \"Hello, World!\"\n```",
            "read_file": "**read_file** - Read file contents\n\n**From**: io\n\n**Syntax**: `read_file with path`\n\n**Returns**: String\n\n**Example**:\n```nlpl\nset content to read_file with \"data.txt\"\n```",
            "write_file": "**write_file** - Write to file\n\n**From**: io\n\n**Syntax**: `write_file with path, content`\n\n**Example**:\n```nlpl\nwrite_file with \"output.txt\", \"Hello\"\n```",
            "input": "**input** - Read user input\n\n**From**: io\n\n**Syntax**: `input with prompt`\n\n**Returns**: String",
            "read_line": "**read_line** - Read single line\n\n**From**: io\n\n**Returns**: String",
            
            # System module
            "exit": "**exit** - Exit program\n\n**From**: system\n\n**Syntax**: `exit with code`\n\n**Example**:\n```nlpl\nexit with 0\n```",
            "get_env": "**get_env** - Get environment variable\n\n**From**: system\n\n**Syntax**: `get_env with name`\n\n**Returns**: String",
            "set_env": "**set_env** - Set environment variable\n\n**From**: system\n\n**Syntax**: `set_env with name, value`",
            "execute": "**execute** - Execute shell command\n\n**From**: system\n\n**Syntax**: `execute with command`\n\n**Returns**: String (output)",
            
            # Collections module
            "sort": "**sort** - Sort collection\n\n**From**: collections\n\n**Syntax**: `sort with collection`\n\n**Returns**: Sorted List",
            "reverse": "**reverse** - Reverse collection\n\n**From**: collections\n\n**Syntax**: `reverse with collection`\n\n**Returns**: Reversed List",
            "filter": "**filter** - Filter collection\n\n**From**: collections\n\n**Syntax**: `filter with collection, predicate`\n\n**Returns**: Filtered List",
            "map": "**map** - Transform collection\n\n**From**: collections\n\n**Syntax**: `map with collection, function`\n\n**Returns**: Transformed List",
            "reduce": "**reduce** - Reduce collection\n\n**From**: collections\n\n**Syntax**: `reduce with collection, function, initial`\n\n**Returns**: Single value",
            
            # Network module
            "http_get": "**http_get** - HTTP GET request\n\n**From**: network\n\n**Syntax**: `http_get with url`\n\n**Returns**: String (response)\n\n**Example**:\n```nlpl\nset response to http_get with \"https://api.example.com\"\n```",
            "http_post": "**http_post** - HTTP POST request\n\n**From**: network\n\n**Syntax**: `http_post with url, data`\n\n**Returns**: String (response)",
            "connect": "**connect** - Create network connection\n\n**From**: network\n\n**Syntax**: `connect with host, port`\n\n**Returns**: Connection",
            "listen": "**listen** - Listen for connections\n\n**From**: network\n\n**Syntax**: `listen with host, port`\n\n**Returns**: Server",
        }
    
    def get_hover(self, text: str, position) -> Optional[Dict]:
        """
        Get hover information at position.
        
        Args:
            text: Document text
            position: Cursor position
            
        Returns:
            Hover information or None
        """
        # Get symbol at position
        symbol = self._get_symbol_at_position(text, position)
        if not symbol:
            return None
        
        # Check documentation database (stdlib/keywords)
        if symbol in self.docs:
            return {
                "contents": {
                    "kind": "markdown",
                    "value": self.docs[symbol]
                }
            }
        
        # Try to infer type/signature from current document
        info = self._infer_symbol_info(text, symbol)
        if info:
            return {
                "contents": {
                    "kind": "markdown",
                    "value": info
                }
            }

        # Try workspace index for user-defined symbols from other files
        info = self._get_info_from_workspace_index(symbol)
        if info:
            return {
                "contents": {
                    "kind": "markdown",
                    "value": info
                }
            }

        return None

    def _get_info_from_workspace_index(self, symbol: str) -> Optional[str]:
        """Get hover info for a user-defined symbol via the workspace index."""
        if not (hasattr(self.server, 'workspace_index') and self.server.workspace_index):
            return None
        symbols = self.server.workspace_index.get_symbol(symbol)
        if not symbols:
            return None
        sym = symbols[0]
        kind_label = {
            'function': 'Function', 'class': 'Class', 'method': 'Method',
            'struct': 'Struct', 'variable': 'Variable', 'field': 'Field',
            'parameter': 'Parameter',
        }.get(sym.kind, 'Symbol')
        info = f"**{sym.name}** \u2014 {kind_label}"
        if sym.signature:
            info += f"\n\n```nlpl\n{sym.kind} {sym.name} {sym.signature}\n```"
        if sym.type_annotation:
            info += f"\n\n**Type**: `{sym.type_annotation}`"
        if sym.doc:
            info += f"\n\n{sym.doc}"
        import os
        file_name = os.path.basename(sym.file_uri.replace('file://', ''))
        info += f"\n\n*Defined in: `{file_name}`*"
        return info
    
    def _get_symbol_at_position(self, text: str, position) -> Optional[str]:
        """Extract symbol at cursor position."""
        lines = text.split('\n')
        if position.line >= len(lines):
            return None
        
        line = lines[position.line]
        if position.character >= len(line):
            return None
        
        # Find word boundaries
        start = position.character
        end = position.character
        
        # Expand left
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        
        # Expand right
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1
        
        symbol = line[start:end]
        return symbol if symbol else None
    
    def _infer_symbol_info(self, text: str, symbol: str) -> Optional[str]:
        """Infer type/signature information for symbol."""
        lines = text.split('\n')
        
        # Look for function definition with full signature
        func_pattern = rf'function\s+{re.escape(symbol)}\s*(<[^>]+>)?\s*(.*?)(?:\n|$)'
        for i, line in enumerate(lines):
            match = re.search(func_pattern, line, re.IGNORECASE)
            if match:
                generic_part = (match.group(1) or '').strip()
                signature = (match.group(2) or '').strip()
                
                # Extract parameters
                params = self._extract_parameters(signature)
                return_type = self._extract_return_type(signature)
                generic_info = self._extract_generic_info(generic_part, signature)
                
                # Build formatted signature
                generic_sig = f"{generic_part} " if generic_part else ""
                info = f"**{symbol}** - Function\n\n```nlpl\nfunction {symbol}{generic_sig}{signature}\n```"
                
                # Add parameter details
                if params:
                    info += "\n\n**Parameters**:\n"
                    for param_name, param_type in params:
                        info += f"- `{param_name}`: {param_type}\n"

                if generic_info:
                    info += f"\n\n{generic_info}"
                
                # Add return type
                if return_type:
                    info += f"\n**Returns**: {return_type}"

                generator_hint = self._extract_generator_flow_hint(lines, i)
                if generator_hint:
                    info += f"\n\n{generator_hint}"
                
                # Look for docstring (comment on next line)
                if i + 1 < len(lines) and lines[i + 1].strip().startswith('#'):
                    docstring = lines[i + 1].strip()[1:].strip()
                    info += f"\n\n{docstring}"
                
                return info
        
        # Look for method definition
        method_pattern = rf'method\s+{re.escape(symbol)}\s*(<[^>]+>)?\s*(.*?)(?:\n|$)'
        for i, line in enumerate(lines):
            match = re.search(method_pattern, line, re.IGNORECASE)
            if match:
                generic_part = (match.group(1) or '').strip()
                signature = (match.group(2) or '').strip()
                params = self._extract_parameters(signature)
                return_type = self._extract_return_type(signature)
                generic_info = self._extract_generic_info(generic_part, signature)
                
                generic_sig = f"{generic_part} " if generic_part else ""
                info = f"**{symbol}** - Method\n\n```nlpl\nmethod {symbol}{generic_sig}{signature}\n```"
                
                if params:
                    info += "\n\n**Parameters**:\n"
                    for param_name, param_type in params:
                        info += f"- `{param_name}`: {param_type}\n"

                if generic_info:
                    info += f"\n\n{generic_info}"
                
                if return_type:
                    info += f"\n**Returns**: {return_type}"

                generator_hint = self._extract_generator_flow_hint(lines, i)
                if generator_hint:
                    info += f"\n\n{generator_hint}"
                
                return info
        
        # Look for class definition with properties
        class_pattern = rf'class\s+{re.escape(symbol)}\b'
        for i, line in enumerate(lines):
            if re.search(class_pattern, line, re.IGNORECASE):
                info = f"**{symbol}** - Class\n\n```nlpl\nclass {symbol}\n```"
                
                # Extract class properties
                properties = []
                j = i + 1
                while j < len(lines) and lines[j].strip() and not lines[j].strip().startswith('end'):
                    prop_match = re.search(r'property\s+(\w+)\s+as\s+(\w+)', lines[j], re.IGNORECASE)
                    if prop_match:
                        prop_name, prop_type = prop_match.groups()
                        properties.append((prop_name, prop_type))
                    j += 1
                
                if properties:
                    info += "\n\n**Properties**:\n"
                    for prop_name, prop_type in properties:
                        info += f"- `{prop_name}`: {prop_type}\n"
                
                return info
        
        # Look for variable with type annotation
        # Finds the first (definition-site) occurrence of this variable name.
        var_pattern = rf'set\s+{re.escape(symbol)}\s+(?:as\s+(\w+)\s+)?to\s+(.+)'
        for line in lines:
            match = re.search(var_pattern, line, re.IGNORECASE)
            if match:
                var_type = match.group(1)
                value = match.group(2).strip()

                info = f"**{symbol}** - Variable"

                if var_type:
                    info += f"\n\n**Type**: {var_type}"

                # Infer type from value if not explicitly typed
                if not var_type:
                    inferred_type = self._infer_type_from_value(value)
                    if inferred_type:
                        info += f"\n\n**Type**: {inferred_type} (inferred)"

                info += f"\n\n**Value**: `{value}`"

                return info
        
        return None
    
    def _extract_parameters(self, signature: str) -> list:
        """Extract parameters from function signature."""
        params = []
        
        # Look for "that takes param1 as Type1, param2 as Type2"
        takes_match = re.search(r'that takes\s+(.+?)(?:\s+returns|$)', signature, re.IGNORECASE)
        if takes_match:
            param_str = takes_match.group(1)
            
            # Split by comma
            param_parts = param_str.split(',')
            for part in param_parts:
                # Extract "param as Type"
                param_match = re.search(r'(\w+)\s+as\s+(\w+(?:\s+of\s+\w+)?)', part.strip(), re.IGNORECASE)
                if param_match:
                    param_name = param_match.group(1)
                    param_type = param_match.group(2)
                    params.append((param_name, param_type))
        
        return params

    def _extract_generic_info(self, generic_part: str, signature: str) -> Optional[str]:
        """Extract and format generic parameter/constraint information."""
        generic_part = (generic_part or '').strip()
        signature = (signature or '').strip()

        params = []
        param_constraints = []
        where_constraints = []

        if generic_part.startswith('<') and generic_part.endswith('>'):
            inner = generic_part[1:-1].strip()
            if inner:
                for token in [p.strip() for p in inner.split(',') if p.strip()]:
                    if ':' in token:
                        param_name, constraint = [x.strip() for x in token.split(':', 1)]
                        params.append(param_name)
                        param_constraints.append(f"- `{param_name}`: {constraint}")
                    else:
                        params.append(token)

        where_match = re.search(r'\bwhere\s+(.+?)(?:\s+returns|$)', signature, re.IGNORECASE)
        if where_match:
            where_clause = where_match.group(1).strip()
            for part in [p.strip() for p in where_clause.split(',') if p.strip()]:
                m = re.match(r'(\w+)\s+is\s+(.+)', part, re.IGNORECASE)
                if m:
                    where_constraints.append(f"- `{m.group(1)}` is {m.group(2).strip()}")
                else:
                    where_constraints.append(f"- {part}")

        if not params and not param_constraints and not where_constraints:
            return None

        info_lines = []
        if params:
            info_lines.append("**Generic Parameters**:")
            for param in params:
                info_lines.append(f"- `{param}`")

        if param_constraints:
            info_lines.append("**Generic Constraints**:")
            info_lines.extend(param_constraints)

        if where_constraints:
            info_lines.append("**Where Constraints**:")
            info_lines.extend(where_constraints)

        return "\n".join(info_lines)
    
    def _extract_return_type(self, signature: str) -> Optional[str]:
        """Extract return type from function signature."""
        returns_match = re.search(r'returns\s+(\w+(?:\s+of\s+\w+)?)', signature, re.IGNORECASE)
        if returns_match:
            return returns_match.group(1)
        return None

    def _extract_generator_flow_hint(self, lines: list, function_line_index: int) -> Optional[str]:
        """Return generator/yield flow hint text for a function, if applicable."""
        body_lines = self._extract_block_body(lines, function_line_index)
        yield_exprs = []
        for raw in body_lines:
            stripped = raw.strip()
            if not stripped.lower().startswith('yield'):
                continue

            expr = stripped[5:].strip()  # text after "yield"
            if not expr:
                expr = "<value>"
            if expr and expr not in yield_exprs:
                yield_exprs.append(expr)

        if not yield_exprs:
            return None

        preview = ", ".join(f"`{expr}`" for expr in yield_exprs[:3])
        if len(yield_exprs) > 3:
            preview += ", ..."

        return (
            "**Generator Flow**:\n"
            f"- Yields: {preview}\n"
            "- Completes when the function reaches `end`"
        )

    def _extract_block_body(self, lines: list, start_index: int) -> list:
        """Extract lines inside a block that starts at ``start_index``."""
        block_starts = re.compile(
            r'^\s*(function|method|if|else if|else|while|for|try|catch|switch|case|default|class|trait|interface|describe|it|test|macro)\b',
            re.IGNORECASE,
        )

        body = []
        depth = 1
        j = start_index + 1

        while j < len(lines):
            line = lines[j]
            stripped = line.strip()

            if re.match(r'^end\b', stripped, re.IGNORECASE):
                depth -= 1
                if depth == 0:
                    break
                body.append(line)
                j += 1
                continue

            if block_starts.match(stripped):
                depth += 1

            body.append(line)
            j += 1

        return body
    
    def _infer_type_from_value(self, value: str) -> Optional[str]:
        """Infer type from variable value."""
        value = value.strip()
        
        # String literals
        if value.startswith('"') and value.endswith('"'):
            return "String"
        if value.startswith("'") and value.endswith("'"):
            return "String"
        
        # Numeric literals
        if value.replace('.', '').replace('-', '').isdigit():
            if '.' in value:
                return "Float"
            else:
                return "Integer"
        
        # Boolean
        if value.lower() in ('true', 'false'):
            return "Boolean"
        
        # List
        if value.startswith('[') and value.endswith(']'):
            return "List"
        
        # Dictionary
        if value.startswith('{') and value.endswith('}'):
            return "Dictionary"
        
        # Class instantiation
        if value.lower().startswith('new '):
            class_match = re.search(r'new\s+(\w+)', value, re.IGNORECASE)
            if class_match:
                return class_match.group(1)
        
        return None


__all__ = ['HoverProvider']
