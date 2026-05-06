# VS Code Marketplace Publishing Guide

## Overview

This guide walks through publishing the NexusLang extension to the Visual Studio Code Marketplace. Publishing requires a Microsoft Azure DevOps account and personal access token.

---

## Prerequisites

- ✅ Extension built and tested locally
- ✅ Extension packaged as `.vsix` file
- ❌ Azure DevOps organization (needed)
- ❌ VS Code Marketplace publisher account (needed)
- ❌ Personal Access Token (PAT) from Azure DevOps (needed)

---

## Step 1: Create Azure DevOps Organization

### 1.1 Sign Up

1. Go to: https://dev.azure.com
2. Sign in with Microsoft account (or create one)
3. Click "Create new organization"
4. Choose organization name (e.g., `nlpl-dev`)
5. Select region (closest to your location)
6. Complete CAPTCHA and create

### 1.2 Verify Organization

```
Organization URL: https://dev.azure.com/your-org-name
```

---

## Step 2: Create Personal Access Token (PAT)

### 2.1 Generate PAT

1. Go to: https://dev.azure.com/your-org-name
2. Click on **User Settings** (gear icon, top right)
3. Select **Personal Access Tokens**
4. Click **+ New Token**

### 2.2 Configure Token

**Name**: `NLPL Extension Publishing`

**Organization**: Select your organization

**Expiration**: 
- Custom defined
- Set to 1 year or longer

**Scopes**: Select **Custom defined**, then:
- ✅ **Marketplace** → **Acquire** (required)
- ✅ **Marketplace** → **Publish** (required)
- ✅ **Marketplace** → **Manage** (optional but recommended)

### 2.3 Save Token

**CRITICAL**: Copy the token immediately - it won't be shown again!

```
Save to secure location:
- Password manager
- Environment variable
- Secure notes
```

---

## Step 3: Create VS Code Marketplace Publisher

### 3.1 Access Publisher Management

1. Go to: https://marketplace.visualstudio.com/manage
2. Sign in with same Microsoft account
3. Click **Create publisher**

### 3.2 Publisher Details

**Publisher ID**: `nlpl-team` (or similar)
- Must be lowercase, alphanumeric, hyphens allowed
- Cannot be changed later
- Will be visible in extension ID: `nlpl-team.nlpl-language-support`

**Publisher Name**: `NLPL Team` (or `The No-hands Company`)
- Display name (can be changed)
- Appears on marketplace listing

**Description**: (optional)
```
NLPL language development tools and IDE support
```

**Logo**: (optional)
- 128x128 PNG recommended
- Appears on marketplace

### 3.3 Verify Publisher Created

You should see your publisher at:
```
https://marketplace.visualstudio.com/publishers/your-publisher-id
```

---

## Step 4: Update package.json

### 4.1 Set Publisher

Update `vscode-extension/package.json`:

```json
{
  "name": "nlpl-language-support",
  "displayName": "NLPL Language Support",
  "publisher": "nlpl-team",  // ← Change from "nlpl" to your publisher ID
  "version": "0.1.0",
  ...
}
```

### 4.2 Verify Repository URL

Ensure repository is correctly set:

```json
{
  "repository": {
    "type": "git",
    "url": "https://github.com/Zajfan/NLPL"
  }
}
```

### 4.3 Add Badges (Optional)

```json
{
  "badges": [
    {
      "url": "https://img.shields.io/visual-studio-marketplace/v/nlpl-team.nlpl-language-support",
      "href": "https://marketplace.visualstudio.com/items?itemName=nlpl-team.nlpl-language-support",
      "description": "Visual Studio Marketplace Version"
    }
  ]
}
```

### 4.4 Add Gallery Banner (Optional)

```json
{
  "galleryBanner": {
    "color": "#1e1e1e",
    "theme": "dark"
  }
}
```

---

## Step 5: Publish Extension

### 5.1 Login with vsce

```bash
cd vscode-extension

# Login with your PAT
npx vsce login your-publisher-id

# When prompted, paste your PAT
# Press Enter
```

**Expected output:**
```
Personal Access Token for publisher 'your-publisher-id': ****
The Personal Access Token verification succeeded for the publisher 'your-publisher-id'.
```

### 5.2 Publish

```bash
# Publish current version
npx vsce publish

# Or publish with version bump
npx vsce publish patch  # 0.1.0 → 0.1.1
npx vsce publish minor  # 0.1.0 → 0.2.0
npx vsce publish major  # 0.1.0 → 1.0.0
```

**Expected output:**
```
Publishing nlpl-team.nlpl-language-support@0.1.0...
Successfully published nlpl-team.nlpl-language-support@0.1.0!
Your extension will live at https://marketplace.visualstudio.com/items?itemName=nlpl-team.nlpl-language-support
```

### 5.3 Verify Publication

1. Visit marketplace URL from output
2. Check extension appears correctly
3. Verify all metadata (description, keywords, etc.)
4. Test installation:

```bash
code --install-extension nlpl-team.nlpl-language-support
```

---

## Step 6: Post-Publication

### 6.1 Update Documentation

Update `README.md` with marketplace installation:

```markdown
## Installation

### From Marketplace
```sh
code --install-extension nlpl-team.nlpl-language-support
```

Or search "NexusLang" in VS Code Extensions view.
```

### 6.2 Tag Release

```bash
git tag -a v1.2.1 -m "Published to VS Code Marketplace"
git push origin v1.2.1
```

### 6.3 Create GitHub Release

1. Go to: https://github.com/Zajfan/NexusLang/releases/new
2. Tag: `v1.2.1`
3. Title: `v1.2.1 - Marketplace Release`
4. Description: Link to marketplace
5. Attach: `nlpl-language-support-0.1.0.vsix`
6. Publish release

---

## Troubleshooting

### Error: "Publisher not found"

**Solution**: Verify publisher ID in package.json matches exactly

### Error: "Extension name already taken"

**Solution**: Change extension name in package.json

### Error: "PAT expired"

**Solution**: Generate new PAT with same scopes

### Error: "Repository not found"

**Solution**: Ensure repository URL is public and accessible

### Warning: "Missing icon"

**Solution**: Add icon to package.json or use `--no-git-tag` flag:
```bash
npx vsce publish --no-git-tag
```

---

## Updating Published Extension

### Update Process

1. Make changes to extension
2. Update version in `package.json`
3. Rebuild: `npm run compile`
4. Test locally
5. Publish:

```bash
npx vsce publish patch  # Auto-increments version
```

### Version Strategy

- **Patch** (0.1.x): Bug fixes, minor updates
- **Minor** (0.x.0): New features, backward compatible
- **Major** (x.0.0): Breaking changes

---

## Security Notes

### PAT Security

- **Never commit PAT to git**
- Store in secure location
- Rotate regularly (every 6-12 months)
- Use minimal required scopes

### Recommended: CI/CD

Set up automated publishing:

```yaml
# .github/workflows/publish-extension.yml
name: Publish Extension

on:
  release:
    types: [published]

jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
      - run: npm ci
      - run: npm run compile
      - run: npx vsce publish -p ${{ secrets.VSCE_PAT }}
```

Store PAT as GitHub secret: `VSCE_PAT`

---

## Marketplace Best Practices

### 1. Quality README

- Clear installation instructions
- Feature screenshots/GIFs
- Configuration examples
- Troubleshooting section

### 2. Changelog

Maintain `CHANGELOG.md`:

```markdown
## [0.1.0] - 2026-02-04
### Added
- Initial release
- 11 LSP features
- Semantic highlighting
```

### 3. Keywords

Use relevant keywords in package.json:
- "nlpl"
- "natural language programming"
- "language server"
- "lsp"
- "syntax highlighting"

### 4. Categories

Choose appropriate categories:
- Programming Languages (primary)
- Linters
- Formatters
- Snippets (if added)

### 5. Icon

Add professional icon (128x128 PNG):
- Represents NexusLang brand
- Clear at small sizes
- Looks good on light/dark backgrounds

---

## Marketplace Statistics

After publishing, track:

- **Installs**: Total installations
- **Downloads**: Total downloads
- **Ratings**: User ratings (1-5 stars)
- **Reviews**: User feedback

Access at: https://marketplace.visualstudio.com/manage/publishers/your-publisher-id

---

## Alternative: Manual Upload

If `vsce publish` fails, manually upload:

1. Package: `npx vsce package`
2. Go to: https://marketplace.visualstudio.com/manage
3. Click **New extension**
4. Upload `.vsix` file
5. Fill in metadata
6. Submit for review

---

## Current Status

**Package Ready**: ✅ `nlpl-language-support-0.1.0.vsix`

**Pending**:
- [ ] Create Azure DevOps organization
- [ ] Generate PAT
- [ ] Create marketplace publisher
- [ ] Update package.json with publisher ID
- [ ] Publish to marketplace

**Estimated Time**: 30 minutes for first-time setup

---

## Quick Start Checklist

- [ ] Create Azure DevOps account
- [ ] Generate PAT (Marketplace: Acquire + Publish)
- [ ] Create publisher on marketplace.visualstudio.com/manage
- [ ] Update package.json with publisher ID
- [ ] Run `npx vsce login your-publisher-id`
- [ ] Run `npx vsce publish`
- [ ] Verify at marketplace URL
- [ ] Test installation: `code --install-extension ...`
- [ ] Update docs with marketplace link
- [ ] Celebrate! 🎉

---

## Next Steps After Publishing

1. Monitor marketplace statistics
2. Respond to user reviews
3. Fix reported issues promptly
4. Release updates regularly
5. Build community around extension

---

## References

- **vsce Documentation**: https://code.visualstudio.com/api/working-with-extensions/publishing-extension
- **Marketplace**: https://marketplace.visualstudio.com
- **Azure DevOps**: https://dev.azure.com
- **Extension Guidelines**: https://code.visualstudio.com/api/references/extension-guidelines

---

## Support

For publishing issues:
- VS Code Extension API: https://code.visualstudio.com/api
- GitHub Discussions: https://github.com/microsoft/vscode-discussions
- Stack Overflow: Tag `vscode-extensions`
