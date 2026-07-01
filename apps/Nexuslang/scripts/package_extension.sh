#!/bin/bash
# Package NexusLang VSCode Extension for Distribution
# Creates .vsix file that can be installed manually or published to marketplace

set -e

echo "📦 Packaging NexusLang VSCode Extension..."

# Change to extension directory
cd "$(dirname "$0")/.vscode/extensions/nlpl"

# Check if vsce is installed
if ! command -v vsce &> /dev/null; then
    echo "⚠️  vsce not found. Installing..."
    npm install -g @vscode/vsce
fi

# Clean previous builds
rm -f *.vsix

# Update icon reference in package.json (use SVG)
if [ -f "icon.svg" ] && ! grep -q '"icon": "icon.svg"' package.json; then
    echo "📝 Updating package.json to use icon.svg..."
    # Already updated in package.json
fi

# Package the extension
echo "🔨 Building extension..."
vsce package

# Find the generated .vsix file
VSIX_FILE=$(ls -t *.vsix | head -n1)

if [ -f "$VSIX_FILE" ]; then
    echo "✅ Extension packaged successfully!"
    echo "📦 Output: $VSIX_FILE"
    echo ""
    echo "📥 Installation options:"
    echo ""
    echo "Option 1 - Install in VSCode:"
    echo "  code --install-extension $VSIX_FILE"
    echo ""
    echo "Option 2 - Install via UI:"
    echo "  1. Open VSCode"
    echo "  2. Go to Extensions (Ctrl+Shift+X)"
    echo "  3. Click '...' → 'Install from VSIX'"
    echo "  4. Select: $(pwd)/$VSIX_FILE"
    echo ""
    echo "Option 3 - Distribute to users:"
    echo "  - Share the .vsix file"
    echo "  - Users run: code --install-extension $VSIX_FILE"
    echo ""
    echo "📤 To publish to marketplace:"
    echo "  1. Create publisher account: https://marketplace.visualstudio.com/manage"
    echo "  2. Get Personal Access Token from Azure DevOps"
    echo "  3. Login: vsce login <publisher-name>"
    echo "  4. Publish: vsce publish"
    echo ""
    
    # Copy to root for easy access
    cp "$VSIX_FILE" ../../../
    echo "📋 Copy placed in project root: ../../../$VSIX_FILE"
else
    echo "❌ Packaging failed!"
    exit 1
fi
