# NexusLang Development Tools - Quick Reference

## Installation

```bash
# From NexusLang project root
pip install -r requirements.txt
```

## Environment Check

```bash
python dev_tools/nxl_dev.py doctor
```

## Common Commands

### Debugging Lexer Issues

```bash
# Visualize tokens
python dev_tools/nxl_dev.py lex myfile.nlpl --visualize

# Token statistics
python dev_tools/nxl_dev.py lex myfile.nlpl --stats

# Dump detailed token info
python dev_tools/nxl_dev.py lex myfile.nlpl --dump
```

### Debugging Parser Issues

```bash
# Show AST tree
python dev_tools/nxl_dev.py parse myfile.nlpl --tree

# Validate syntax
python dev_tools/nxl_dev.py parse myfile.nlpl --validate

# Export AST as JSON
python dev_tools/nxl_dev.py parse myfile.nlpl --json --output ast.json
```

### Debugging Runtime Issues

```bash
# Interactive debugger
python dev_tools/nxl_dev.py debug myfile.nlpl --interactive

# Trace execution
python dev_tools/nxl_dev.py run myfile.nlpl --trace

# Show scope state
python dev_tools/nxl_dev.py run myfile.nlpl --scope
```

### Full Debugging

```bash
# See everything
python dev_tools/nxl_dev.py run myfile.nlpl --debug
```

## Interactive Debugger Commands

When in interactive mode (`--interactive`):

- `n` or `next` - Execute next statement
- `s` or `scope` - Show scope hierarchy  
- `v <name>` - Show variable value
- `c` or `continue` - Continue execution
- `q` or `quit` - Quit debugger

## Typical Workflow

1. **Environment check**: `python dev_tools/nxl_dev.py doctor`
2. **Test lexer**: `python dev_tools/nxl_dev.py lex test.nlpl --visualize`
3. **Test parser**: `python dev_tools/nxl_dev.py parse test.nlpl --tree`
4. **Test execution**: `python dev_tools/nxl_dev.py run test.nlpl --debug`

## Files

- `nxl_dev.py` - Main CLI entry point
- `lexer_tools/token_debugger.py` - Lexer debugging
- `parser_tools/ast_debugger.py` - Parser debugging
- `interpreter_tools/execution_debugger.py` - Execution debugging
- `README.md` - Full documentation

## Getting Help

```bash
python dev_tools/nxl_dev.py help
```

For detailed usage, see `dev_tools/README.md`
