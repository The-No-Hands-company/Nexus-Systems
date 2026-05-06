# NexusLang Language Support for VS Code

Language support extension for NexusLang (NexusLang) with full LSP features.

## Features

- **Syntax Highlighting**: Natural language keywords, types, operators
- **IntelliSense**: Code completion with suggestions
- **Go to Definition**: Navigate to symbol definitions
- **Find References**: Find all references to a symbol
- **Hover Information**: Type and documentation on hover
- **Diagnostics**: Real-time error and warning reporting
- **Document Symbols**: Outline view of functions, classes, methods
- **Code Actions**: Quick fixes and refactorings
- **Semantic Tokens**: Semantic highlighting for symbols and control-flow keywords
- **Rename Refactoring**: Symbol-aware rename through the language server

## Requirements

The extension needs access to the NexusLang language server runtime.

If you are working in this repository, no package install is required. Use a Python
environment with dependencies installed:

```bash
pip install -r requirements.txt
```

If you are using the extension outside this repository, either:

- set `nexuslang.languageServer.path` to a language-server executable/script, or
- use a Python environment where `python -m nexuslang.lsp --stdio` is available.

## Extension Settings

This extension contributes the following settings:

- `nexuslang.languageServer.enabled`: Enable/disable the language server (default: `true`)
- `nexuslang.languageServer.path`: Path to language server executable (leave empty to use `python -m nexuslang.lsp`)
- `nexuslang.languageServer.pythonPath`: Python interpreter used to launch the server
- `nexuslang.languageServer.arguments`: Arguments passed to the server (default: `["--stdio"]`)
- `nexuslang.languageServer.debug`: Enable debug mode (default: `false`)
- `nexuslang.languageServer.logFile`: Path to log file for server output
- `nexuslang.languageServer.linting.enabled`: Enable real-time lint diagnostics
- `nexuslang.languageServer.linting.strict`: Use strict lint profile in the editor
- `nexuslang.languageServer.linting.errorsOnly`: Only show lint diagnostics with error severity
- `nexuslang.trace.server`: Trace communication between client and server (`off`, `messages`, `verbose`)

## Usage

1. Install the extension
2. Open a `.nxl` file
3. The language server will start automatically
4. Start coding with IntelliSense support

## Examples

```nlpl
# Function with type annotations
function calculate_average with numbers as List of Integer returns Float
    if numbers is empty
        return 0.0
    end
    
    set total to 0
    for each num in numbers
        set total to total plus num
    end
    
    return total divided by length of numbers
end

# Class with properties and methods
class Rectangle
    property width as Integer
    property height as Integer
    
    method area returns Integer
        return width times height
    end
    
    method perimeter returns Integer
        return 2 times (width plus height)
    end
end
```

## Development

To build the extension from source:

```bash
cd vscode-extension
npm install
npm run compile
```

To package the extension:

```bash
npm run package
```

To publish (requires publisher account):

```bash
npm run publish
```

## Known Issues

- Some advanced features are still in development
- Full debugger polish is still in progress
- Some advanced refactorings are not yet implemented

## Release Notes

### 0.1.0

Initial release with:
- Syntax highlighting
- LSP integration
- Semantic tokens, document symbols, and rename support

## Contributing

Contributions are welcome! Visit [https://github.com/Zajfan/NLPL](https://github.com/Zajfan/NLPL)

## License

MIT
