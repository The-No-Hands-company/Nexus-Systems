#!/bin/bash
# Install NexusLang VSCode Extension Globally
# This makes the extension available in ALL VSCode projects

set -e

echo "🚀 Installing NexusLang Extension Globally..."

# Determine VSCode extensions directory
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    VSCODE_EXTENSIONS="$HOME/.vscode/extensions"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    VSCODE_EXTENSIONS="$HOME/.vscode/extensions"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    VSCODE_EXTENSIONS="$USERPROFILE/.vscode/extensions"
else
    echo "❌ Unsupported OS: $OSTYPE"
    exit 1
fi

# Create extensions directory if it doesn't exist
mkdir -p "$VSCODE_EXTENSIONS"

# Copy extension
EXTENSION_DIR="$VSCODE_EXTENSIONS/nexuslang-language-support"
echo "📦 Installing to: $EXTENSION_DIR"

if [ -d "$EXTENSION_DIR" ]; then
    echo "⚠️  Extension already exists. Removing old version..."
    rm -rf "$EXTENSION_DIR"
fi

cp -r .vscode/extensions/nlpl "$EXTENSION_DIR"

echo "✅ Extension installed successfully!"
echo ""
echo "📝 Next steps:"
echo "1. Reload VSCode (Ctrl+Shift+P → 'Developer: Reload Window')"
echo "2. Extension will auto-activate for .nlpl files in ANY project"
echo ""
echo "⚙️  Configuration:"
echo "To use in a new project, create .vscode/settings.json:"
echo '{'
echo '  "nexuslang.languageServer.enabled": true,'
echo '  "nexuslang.languageServer.path": "/path/to/nlpl/src/nxl_lsp.py"'
echo '}'
