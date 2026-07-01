#!/bin/bash
# Quick setup script for NexusLang VSCode extension

echo "=========================================="
echo "NLPL VSCode Extension Setup"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "src/nxl_lsp.py" ]; then
    echo "❌ Error: Please run this from the NexusLang project root"
    exit 1
fi

# Get absolute path to project root
PROJECT_ROOT=$(pwd)
EXTENSION_DIR=".vscode/nexuslang-extension"

echo "Project root: $PROJECT_ROOT"
echo "Extension directory: $EXTENSION_DIR"
echo ""

# Option 1: Install extension in development mode (recommended for this project)
echo "Option 1: Development Mode (Recommended)"
echo "==========================================="
echo ""
echo "This allows you to use the extension in THIS workspace without packaging."
echo ""
echo "Steps:"
echo "1. Open Command Palette (Ctrl+Shift+P)"
echo "2. Type: 'NexusLang' and the extension should auto-activate for .nlpl files"
echo "3. Open any .nlpl file in examples/ or test_programs/"
echo ""
echo "The extension will automatically:"
echo "  ✓ Activate syntax highlighting"
echo "  ✓ Start the LSP server at: $PROJECT_ROOT/src/nxl_lsp.py"
echo "  ✓ Provide diagnostics, completions, signature help, etc."
echo ""
echo "Extension is configured in: .vscode/settings.json"
echo ""

# Option 2: Build and install globally
echo "Option 2: Build and Install Globally"
echo "==========================================="
echo ""
echo "To package and install the extension globally:"
echo ""
echo "cd $EXTENSION_DIR"
echo "npm install"
echo "npm run compile"
echo "npm install -g @vscode/vsce"
echo "vsce package"
echo "code --install-extension nexuslang-language-support-0.1.0.vsix"
echo ""

# Check if TypeScript compiler is available
echo "Checking dependencies..."
echo ""

if command -v node &> /dev/null; then
    echo "✓ Node.js found: $(node --version)"
else
    echo "✗ Node.js not found (needed for building extension)"
    echo "  Install from: https://nodejs.org/"
fi

if command -v npm &> /dev/null; then
    echo "✓ npm found: $(npm --version)"
else
    echo "✗ npm not found (needed for building extension)"
fi

if command -v python3 &> /dev/null; then
    echo "✓ Python 3 found: $(python3 --version)"
else
    echo "✗ Python 3 not found (required for LSP server)"
fi

echo ""
echo "=========================================="
echo "Quick Test"
echo "=========================================="
echo ""
echo "To test the LSP server directly:"
echo "  python src/nxl_lsp.py"
echo ""
echo "To test diagnostics:"
echo "  python dev_tools/test_lsp_diagnostics.py"
echo ""
echo "To test enhanced features:"
echo "  python dev_tools/test_lsp_enhanced.py"
echo ""
echo "=========================================="
echo "Ready to use NLPL!"
echo "=========================================="
echo ""
echo "Just open any .nlpl file and the extension should activate automatically."
echo "Check the Output panel (View > Output) and select 'NLPL Language Server' to see logs."
echo ""
