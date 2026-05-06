# NexusLang Development Tools

A comprehensive suite of debugging and development utilities for the NexusLang (NexusLang).

## Overview

The dev_tools suite provides powerful debugging capabilities across all phases of NexusLang compilation and execution:

- **Lexer Tools**: Token visualization, statistics, and keyword analysis
- **Parser Tools**: AST visualization, syntax validation, error analysis
- **Interpreter Tools**: Execution tracing, scope inspection, interactive debugging
- **Unified CLI**: Single command-line interface for all tools

## Installation

Install the required dependencies:

```bash
pip install colorama  # For colored output (optional but recommended)
```

All other dependencies are already in the main NexusLang requirements.txt.

## Quick Start

### Using the Unified CLI

The `nlpl-dev` command provides access to all tools:

```bash
# Check your environment
python dev_tools/nxl_dev.py doctor

# Visualize tokens
python dev_tools/nxl_dev.py lex examples/01_basic_concepts.nlpl --visualize

# View AST tree
python dev_tools/nxl_dev.py parse examples/01_basic_concepts.nlpl --tree

# Interactive debugging
python dev_tools/nxl_dev.py debug test_hello.nlpl --interactive

# Run with full debugging
python dev_tools/nxl_dev.py run test_hello.nlpl --debug
```

## Tool Categories

### 1. Lexer Tools (`lexer_tools/`)

**Token Debugger** - Analyze tokenization phase

```bash
# Visualize tokens with colors
python dev_tools/lexer_tools/token_debugger.py myfile.nlpl --visualize

# Get detailed token dump
python dev_tools/lexer_tools/token_debugger.py myfile.nlpl --dump

# Show token statistics
python dev_tools/lexer_tools/token_debugger.py myfile.nlpl --stats

# Check keyword registry
python dev_tools/lexer_tools/token_debugger.py --keywords

# Dump to file
python dev_tools/lexer_tools/token_debugger.py myfile.nlpl --dump --output tokens.txt
```

**Features:**
- Color-coded token visualization by type
- Detailed token information (line, column, type, lexeme, literal)
- Token statistics and distribution analysis
- Keyword registry inspection
- Export to file for further analysis

### 2. Parser Tools (`parser_tools/`)

**AST Debugger** - Analyze parsing and AST generation

```bash
# Show tree view of AST
python dev_tools/parser_tools/ast_debugger.py myfile.nlpl --tree

# Export AST as JSON
python dev_tools/parser_tools/ast_debugger.py myfile.nlpl --json --output ast.json

# Validate syntax
python dev_tools/parser_tools/ast_debugger.py myfile.nlpl --validate

# Show tokens before parsing
python dev_tools/parser_tools/ast_debugger.py myfile.nlpl --tree --show-tokens
```

**Features:**
- Tree visualization of AST structure
- JSON export for programmatic analysis
- Syntax validation with suggestions
- Parse error analysis with context
- Token stream preview

### 3. Interpreter Tools (`interpreter_tools/`)

**Execution Debugger** - Debug program execution

```bash
# Interactive step-through debugger
python dev_tools/interpreter_tools/execution_debugger.py myfile.nlpl --interactive

# Trace execution
python dev_tools/interpreter_tools/execution_debugger.py myfile.nlpl --trace

# Show final scope state
python dev_tools/interpreter_tools/execution_debugger.py myfile.nlpl --scope
```

**Interactive Debugger Commands:**
```
n/next      - Execute next statement
s/scope     - Show scope hierarchy
v <name>    - Show variable value
c/continue  - Continue execution
q/quit      - Quit debugger
```

**Features:**
- Step-by-step execution
- Scope hierarchy visualization
- Variable inspection at any point
- Execution tracing with statistics
- Breakpoint support (coming soon)

## Use Cases

### Debugging Lexer Issues

When you suspect tokenization problems:

```bash
# First, visualize the tokens
python dev_tools/nxl_dev.py lex myfile.nlpl --visualize

# Get detailed statistics
python dev_tools/nxl_dev.py lex myfile.nlpl --stats

# Check keyword coverage
python dev_tools/nxl_dev.py lex myfile.nlpl --keywords
```

### Debugging Parser Issues

When you get parse errors:

```bash
# Validate syntax first
python dev_tools/nxl_dev.py parse myfile.nlpl --validate

# Show AST to see where parsing stops
python dev_tools/nxl_dev.py parse myfile.nlpl --tree --show-tokens

# Export for detailed analysis
python dev_tools/nxl_dev.py parse myfile.nlpl --json --output debug.json
```

### Debugging Runtime Issues

When your program doesn't execute correctly:

```bash
# Use interactive debugger
python dev_tools/nxl_dev.py debug myfile.nlpl --interactive

# Or trace execution
python dev_tools/nxl_dev.py run myfile.nlpl --trace --scope
```

### Full Pipeline Debugging

When you need to see everything:

```bash
# Run with all debug options
python dev_tools/nxl_dev.py run myfile.nlpl --debug
```

This shows:
- Token stream
- AST tree
- Execution trace
- Scope state
- Statistics

## Development Workflow

### 1. Starting a New Feature

```bash
# Check environment
python dev_tools/nxl_dev.py doctor

# Create test file
echo "set x to 42" > test_feature.nlpl

# Test lexer first
python dev_tools/nxl_dev.py lex test_feature.nlpl --visualize

# Test parser
python dev_tools/nxl_dev.py parse test_feature.nlpl --tree

# Test execution
python dev_tools/nxl_dev.py run test_feature.nlpl --debug
```

### 2. Debugging a Failing Example

```bash
# Full debug of example file
python dev_tools/nxl_dev.py run examples/01_basic_concepts.nlpl --debug

# If it fails at lexing
python dev_tools/nxl_dev.py lex examples/01_basic_concepts.nlpl --dump --output lex_debug.txt

# If it fails at parsing
python dev_tools/nxl_dev.py parse examples/01_basic_concepts.nlpl --tree --show-tokens

# If it fails at runtime
python dev_tools/nxl_dev.py debug examples/01_basic_concepts.nlpl --interactive
```

### 3. Verifying a Fix

```bash
# Test with full tracing
python dev_tools/nxl_dev.py run myfile.nlpl --trace --scope --stats
```

## Advanced Features

### Creating Custom Debug Scripts

You can import the tools in your own scripts:

```python
from dev_tools.lexer_tools.token_debugger import TokenVisualizer, TokenStatistics
from dev_tools.parser_tools.ast_debugger import ASTVisualizer
from dev_tools.interpreter_tools.execution_debugger import ScopeInspector

# Your custom debugging logic here
```

### Automated Testing Integration

Use dev tools in your test suite:

```python
def test_my_feature():
    from dev_tools.parser_tools.ast_debugger import SyntaxValidator
    
    source = "set x to 42"
    issues = SyntaxValidator.validate(source)
    assert len(issues) == 0
```

## Tips and Tricks

1. **Use `doctor` regularly** - Catch environment issues early
   ```bash
   python dev_tools/nxl_dev.py doctor
   ```

2. **Start with the simplest tool** - Debug from lexer → parser → interpreter

3. **Export to files** - Save debug output for comparison
   ```bash
   python dev_tools/nxl_dev.py lex myfile.nlpl --dump --output before.txt
   # Make changes
   python dev_tools/nxl_dev.py lex myfile.nlpl --dump --output after.txt
   diff before.txt after.txt
   ```

4. **Use interactive mode for complex issues** - Step through execution
   ```bash
   python dev_tools/nxl_dev.py debug myfile.nlpl --interactive
   ```

5. **Combine multiple flags** - Get exactly the information you need
   ```bash
   python dev_tools/nxl_dev.py run myfile.nlpl --trace --scope
   ```

## Future Enhancements

- [ ] Breakpoint support in interactive debugger
- [ ] Memory inspector for runtime
- [ ] Performance profiling tools
- [ ] Code coverage analysis
- [ ] Automated test case generation
- [ ] Visual debugger GUI
- [ ] Integration with VS Code debugger protocol

## Contributing

When adding new debug tools:

1. Create tool in appropriate category directory
2. Add CLI integration in `nxl_dev.py`
3. Update this README
4. Add examples of usage

## Troubleshooting

**Tool won't run:**
- Check Python version: `python --version` (need 3.8+)
- Run `python dev_tools/nxl_dev.py doctor`
- Install dependencies: `pip install -r requirements.txt`

**Import errors:**
- Make sure you're running from project root
- Check `sys.path` includes project root

**No colored output:**
- Install colorama: `pip install colorama`
- Works without it, just less visually appealing

## License

Same as main NexusLang project.
