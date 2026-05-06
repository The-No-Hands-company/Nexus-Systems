# NexusLang Deployment Guide

Complete guide to deploying NexusLang as a production-ready programming language.

## Table of Contents

1. [VSCode Extension Distribution](#vscode-extension-distribution)
2. [Other Editor Support](#other-editor-support)
3. [Language Server Distribution](#language-server-distribution)
4. [NLPL CLI Distribution](#nlpl-cli-distribution)
5. [Package Managers](#package-managers)

---

## 1. VSCode Extension Distribution

### 1.1 Publish to VSCode Marketplace (Official)

**Prerequisites:**
- VSCode publisher account
- `vsce` (VSCode Extension CLI)

```bash
# Install vsce
npm install -g @vscode/vsce

# Package extension
cd .vscode/extensions/nlpl
vsce package
# Creates: nlpl-language-support-0.1.0.vsix

# Publish to marketplace
vsce publish
```

**Setup Publisher Account:**
1. Go to https://marketplace.visualstudio.com/manage
2. Create publisher: `nlpl-lang` or your organization
3. Generate Personal Access Token (Azure DevOps)
4. Login: `vsce login <publisher-name>`

**Update package.json before publishing:**
```json
{
 "publisher": "nlpl-lang",
 "repository": {
 "type": "git",
 "url": "https://github.com/Zajfan/NLPL.git"
 },
 "bugs": {
 "url": "https://github.com/Zajfan/NexusLang/issues"
 },
 "homepage": "https://github.com/Zajfan/NLPL#readme",
 "keywords": ["nlpl", "natural-language", "programming", "language-server"]
}
```

**Once published, users install via:**
```bash
code --install-extension nlpl-lang.nlpl-language-support
```
Or search "NexusLang" in VSCode Extensions marketplace.

### 1.2 Manual Distribution (.vsix file)

```bash
# Create distributable package
cd .vscode/extensions/nlpl
vsce package --out nlpl-extension.vsix

# Users install with:
code --install-extension nlpl-extension.vsix
```

**Distribution channels:**
- GitHub Releases (attach .vsix)
- Project website downloads
- Internal enterprise repositories

---

## 2. Other Editor Support

### 2.1 Neovim/Vim (via LSP)

**Native LSP (Neovim 0.5+):**

Create `~/.config/nvim/lua/nlpl.lua`:
```lua
local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Register NexusLang LSP
if not configs.nlpl then
 configs.nlpl = {
 default_config = {
 cmd = {'python3', '/path/to/NexusLang/src/nxl_lsp.py'},
 filetypes = {'nlpl'},
 root_dir = lspconfig.util.root_pattern('.git', 'main.nxl'),
 settings = {},
 },
 }
end

lspconfig.nlpl.setup{}

-- Set filetype
vim.cmd([[
 augroup nlpl
 autocmd!
 autocmd BufRead,BufNewFile *.nlpl set filetype=nlpl
 augroup END
]])
```

**CoC.nvim (Vim/Neovim):**

Add to `~/.vim/coc-settings.json`:
```json
{
 "languageserver": {
 "nlpl": {
 "command": "python3",
 "args": ["/path/to/NexusLang/src/nxl_lsp.py"],
 "filetypes": ["nlpl"],
 "rootPatterns": [".git/", "main.nxl"]
 }
 }
}
```

### 2.2 Sublime Text (via LSP)

**Install LSP package:**
- Install Package Control
- Install "LSP" package

**Configure LSP-nlpl** (`Preferences` `Package Settings` `LSP` `Settings`):
```json
{
 "clients": {
 "nlpl": {
 "enabled": true,
 "command": ["python3", "/path/to/NexusLang/src/nxl_lsp.py"],
 "selector": "source.nxl",
 "syntaxes": ["Packages/User/NLPL.sublime-syntax"]
 }
 }
}
```

**Create syntax file** (`Packages/User/NLPL.sublime-syntax`):
```yaml
%YAML 1.2
---
name: NexusLang
file_extensions: [nlpl]
scope: source.nlpl

contexts:
 main:
 - match: '\b(function|set|to|if|else|while|for|each|in|return|class|struct|union)\b'
 scope: keyword.control.nlpl
 - match: '\b(Integer|String|Float|Boolean|List|Dict)\b'
 scope: storage.type.nlpl
 - match: '"[^"]*"'
 scope: string.quoted.double.nlpl
 - match: '#.*$'
 scope: comment.line.number-sign.nlpl
```

### 2.3 Emacs (via lsp-mode)

**Setup lsp-mode:**

Add to `~/.emacs` or `~/.emacs.d/init.el`:
```elisp
(require 'lsp-mode)

;; Define NexusLang LSP client
(lsp-register-client
 (make-lsp-client
 :new-connection (lsp-stdio-connection
 '("python3" "/path/to/NexusLang/src/nxl_lsp.py"))
 :major-modes '(nlpl-mode)
 :server-id 'nlpl-lsp))

;; Define NexusLang major mode
(define-derived-mode nlpl-mode prog-mode "NexusLang"
 "Major mode for NexusLang."
 (setq-local comment-start "# ")
 (setq-local comment-end ""))

;; Auto-activate for .nlpl files
(add-to-list 'auto-mode-alist '("\\.nxl\\'" . nlpl-mode))

;; Enable LSP for NexusLang
(add-hook 'nlpl-mode-hook #'lsp)
```

### 2.4 JetBrains IDEs (IntelliJ, PyCharm, etc.)

**Create Custom Language Plugin:**

Requires IntelliJ Plugin Development Kit. Structure:
```
nlpl-intellij-plugin/
 src/
 main/
 java/
 com/nlpl/
 NLPLLanguage.java
 NLPLFileType.java
 NLPLSyntaxHighlighter.java
 resources/
 META-INF/plugin.xml
 icons/nlpl.png
 build.gradle
```

**plugin.xml:**
```xml
<idea-plugin>
 <id>com.nlpl.language</id>
 <name>NLPL Language Support</name>
 <vendor>NLPL Team</vendor>
 <description>Support for NexusLang programming language</description>
 
 <depends>com.intellij.modules.platform</depends>
 
 <extensions defaultExtensionNs="com.intellij">
 <fileType name="NexusLang" 
 implementationClass="com.nlpl.NLPLFileType" 
 fieldName="INSTANCE" 
 language="NexusLang" 
 extensions="nlpl"/>
 <lang.syntaxHighlighterFactory 
 language="NexusLang" 
 implementationClass="com.nlpl.NLPLSyntaxHighlighterFactory"/>
 </extensions>
</idea-plugin>
```

---

## 3. Language Server Distribution

### 3.1 Standalone LSP Server Package

**Create Python package:**

```bash
# pyproject.toml additions for LSP server
[project.scripts]
nlpl-lsp = "nexuslang.lsp.server:main"

# Install as system service
pip install -e .
nlpl-lsp # Now available globally
```

**Docker Container:**

Create `Dockerfile.lsp`:
```dockerfile
FROM python:3.11-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY src/ /app/src/
ENV PYTHONPATH=/app

EXPOSE 2087

CMD ["python3", "-m", "nexuslang.lsp.server"]
```

Build and run:
```bash
docker build -f Dockerfile.lsp -t nlpl-lsp:latest .
docker run -p 2087:2087 nlpl-lsp:latest
```

Editors connect via TCP: `tcp://localhost:2087`

### 3.2 Language Server Protocol (LSP) Best Practices

**Versioning:**
- Use semantic versioning (MAJOR.MINOR.PATCH)
- Maintain backward compatibility for MINOR versions
- Document breaking changes in MAJOR releases

**Performance:**
- Implement incremental document sync
- Cache parsed ASTs and type information
- Use background threads for heavy operations
- Implement request cancellation

**Reliability:**
- Graceful error handling (never crash editor)
- Comprehensive logging (configurable verbosity)
- Health checks and diagnostics
- Recovery from invalid states

---

## 4. NexusLang CLI Distribution

### 4.1 System-Wide Installation

**Option A: pip install (Recommended):**

```bash
# From PyPI (once published)
pip install nlpl

# Development install
pip install -e .

# Now available globally:
nlpl run script.nlpl
nlpl compile script.nlpl
nlpl test tests/
```

**Option B: Binary Distribution:**

Use PyInstaller to create standalone executables:

```bash
# Install PyInstaller
pip install pyinstaller

# Create standalone binary
pyinstaller --onefile \
 --name nlpl \
 --add-data "src/nlpl:nlpl" \
 src/main.py

# Result: dist/nlpl (Linux/Mac) or dist/nlpl.exe (Windows)
```

**Distribution:**
- GitHub Releases (multi-platform binaries)
- Homebrew (Mac): Create formula
- APT/RPM packages (Linux)
- Chocolatey (Windows)

### 4.2 Package Manager Integration

**Homebrew Formula** (`nlpl.rb`):
```ruby
class Nlpl < Formula
 desc "NexusLang"
 homepage "https://github.com/Zajfan/NLPL"
 url "https://github.com/Zajfan/NexusLang/archive/v1.0.0.tar.gz"
 sha256 "..."
 license "MIT"

 depends_on "python@3.11"

 def install
 virtualenv_install_with_resources
 end

 test do
 system "#{bin}/nlpl", "--version"
 end
end
```

**Installation:**
```bash
brew tap nlpl-lang/nlpl
brew install nlpl
```

**APT Package (Debian/Ubuntu):**

Create `debian/` directory structure:
```
debian/
 control
 rules
 changelog
 compat
```

Build:
```bash
dpkg-buildpackage -us -uc
sudo dpkg -i ../nxl_1.0.0_all.deb
```

---

## 5. Complete Distribution Strategy

### 5.1 Release Checklist

**Pre-Release:**
- [ ] Update version in `pyproject.toml`, `package.json`
- [ ] Run full test suite: `pytest tests/`
- [ ] Update `CHANGELOG.md`
- [ ] Update documentation
- [ ] Test on all platforms (Linux, Mac, Windows)
- [ ] Verify LSP server works with all editors

**Release Process:**
1. **Tag release:** `git tag -a v1.0.0 -m "Release 1.0.0"`
2. **Push tag:** `git push origin v1.0.0`
3. **GitHub Release:**
 - Create release from tag
 - Attach: `.vsix` file, binaries, source archives
 - Write release notes

4. **Publish to registries:**
 ```bash
 # PyPI
 python -m build
 twine upload dist/*
 
 # VSCode Marketplace
 cd .vscode/extensions/nlpl
 vsce publish
 
 # npm (if Node.js components)
 npm publish
 ```

### 5.2 Installation Quick Start

**For Users (Production):**

```bash
# 1. Install NexusLang CLI
pip install nlpl

# 2. Install VSCode Extension
code --install-extension nlpl-lang.nlpl-language-support

# 3. Verify installation
nlpl --version
nlpl run hello.nlpl

# 4. Start developing!
```

**For Developers (Contributing):**

```bash
# 1. Clone repository
git clone https://github.com/Zajfan/NLPL.git
cd NexusLang

# 2. Install development dependencies
pip install -e ".[dev]"

# 3. Install VSCode extension locally
./install_extension_globally.sh

# 4. Run tests
pytest tests/

# 5. Start developing!
```

### 5.3 Distribution Platforms

| Platform | Installation Command | Status |
|----------|---------------------|--------|
| **PyPI** | `pip install nlpl` | Pending |
| **VSCode Marketplace** | Search "NexusLang" in Extensions | Pending |
| **Homebrew** | `brew install nlpl` | Pending |
| **APT** | `apt install nlpl` | Pending |
| **Chocolatey** | `choco install nlpl` | Pending |
| **Snap** | `snap install nlpl` | Pending |
| **Docker Hub** | `docker pull nlpl/nlpl` | Pending |

### 5.4 Documentation Website

**Recommended stack:**
- **Static site generator:** Docusaurus, MkDocs, or Sphinx
- **Hosting:** GitHub Pages, Netlify, or Vercel
- **Domain:** `nlpl-lang.org` or similar

**Content structure:**
```
docs/
 getting-started/
 installation.md
 quickstart.md
 hello-world.md
 language-guide/
 syntax.md
 types.md
 stdlib.md
 editor-setup/
 vscode.md
 vim.md
 emacs.md
 api-reference/
 examples/
```

---

## 6. Next Steps for Production Readiness

### Immediate (Week 1-2):
1. Create global install script (done)
2. Update extension `package.json` with publisher info
3. Test extension in multiple VSCode versions
4. Create `.vsix` package for manual distribution

### Short-term (Month 1):
1. Register VSCode publisher account
2. Publish to VSCode Marketplace
3. Create PyPI package
4. Write comprehensive installation docs
5. Create demonstration videos

### Medium-term (Month 2-3):
1. Add Neovim/Vim LSP configuration
2. Create Sublime Text package
3. Build standalone binaries with PyInstaller
4. Create Homebrew formula
5. Setup CI/CD for automated releases

### Long-term (Month 4+):
1. JetBrains plugin development
2. APT/RPM package repositories
3. Docker Hub images
4. Documentation website
5. Package manager integration (Snap, Chocolatey)

---

## 7. Support & Community

**Before going public:**
- Set up GitHub Discussions
- Create Discord/Slack community
- Write contributing guidelines
- Establish code of conduct
- Define support channels

**Launch strategy:**
- Blog announcement
- Reddit posts (r/programming, r/ProgrammingLanguages)
- Hacker News submission
- Twitter/social media campaign
- Submit to programming language lists

---

## Contact

- **Repository:** https://github.com/Zajfan/NLPL
- **Issues:** https://github.com/Zajfan/NexusLang/issues
- **Documentation:** (Coming soon)
- **Discord:** (Coming soon)

---

**Last updated:** January 5, 2026
