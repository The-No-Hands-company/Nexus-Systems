# NexusLang REPL Quick Reference

## Starting the REPL

```bash
python -m nexuslang.main # Start REPL
python -m nexuslang.main --debug # Start with debug mode
python nxl_repl.py # Alternative entry point
```

## Special Commands

| Command | Description |
|---------|-------------|
| `:help` | Show help |
| `:quit` / `:exit` | Exit REPL |
| `:vars` | Show variables |
| `:funcs` | Show functions |
| `:clear` | Clear screen |
| `:reset` | Reset interpreter |
| `:history` | Show command history |
| `:debug` | Toggle debug mode |
| `:type-check` | Toggle type checking |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Tab` | Auto-complete |
| `` | Previous command |
| `` | Next command |
| `Ctrl+C` | Interrupt (doesn't exit) |
| `Ctrl+D` | Exit REPL |
| `Ctrl+L` | Clear screen |

## Multi-line Input

REPL automatically detects incomplete statements:

```nexuslang
>>> function greet with name as String
... return "Hello, " plus name
... end
```

Press Enter on empty line to execute.

## Quick Examples

### Variables
```nexuslang
>>> set x to 42
>>> set name to "NexusLang"
```

### Functions
```nexuslang
>>> function add with a as Integer, b as Integer returns Integer
... return a plus b
... end
>>> add with 5, 3
=> 8
```

### Loops
```nexuslang
>>> for each num in [1, 2, 3]
... print text num
... end
```

### Structs
```nexuslang
>>> struct Point
... x as Integer
... y as Integer
... end
>>> set p to new Point
>>> set p.x to 10
```

## Tips

1. **Tab completion**: Type partial keyword/variable and press Tab
2. **History**: Use / to navigate previous commands
3. **Reset**: Use `:reset` if state gets messy
4. **Debug**: Enable `:debug` to see tokens/AST
5. **Inspect**: Use `:vars` and `:funcs` frequently

## Error Recovery

If you get an error, the REPL continues running:

```nexuslang
>>> set x to "invalid" plus 42
Error: Cannot add string and integer
>>> set y to 100
=> 100
```

## See Also

- Full documentation: `docs/tooling/repl.md`
- Examples: `examples/01_basic_concepts.nlpl`
- Manual tests: `test_repl_manual.py`
