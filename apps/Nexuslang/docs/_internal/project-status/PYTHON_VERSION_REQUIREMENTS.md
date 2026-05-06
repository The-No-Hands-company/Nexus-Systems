# Python Version Requirements for NexusLang

## Supported Python Versions

- **Python 3.11**: Fully supported, recommended
- **Python 3.12**: Fully supported, recommended 
- **Python 3.13**: Fully supported, recommended
- **Python 3.14**: **NOT SUPPORTED** due to import system regression

## Installation Guide

### Arch Linux / Manjaro

```bash
# Install Python 3.13
sudo pacman -S python313

# Verify installation
python3.13 --version

# Use for NexusLang
python3.13 nlplc_llvm.py program.nlpl -o output
```

### Ubuntu / Debian

```bash
# Add deadsnakes PPA (for newer Python versions)
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update

# Install Python 3.13
sudo apt install python3.13 python3.13-venv python3.13-dev

# Use for NexusLang
python3.13 nlplc_llvm.py program.nlpl -o output
```

### Fedora / RHEL

```bash
# Install Python 3.13
sudo dnf install python3.13

# Use for NexusLang
python3.13 nlplc_llvm.py program.nlpl -o output
```

### macOS

```bash
# Using Homebrew
brew install python@3.13

# Use for NexusLang
python3.13 nlplc_llvm.py program.nlpl -o output
```

## Virtual Environment Setup

```bash
# Create venv with specific Python version
python3.13 -m venv venv

# Activate
source venv/bin/activate # Linux/macOS
# or
venv\Scripts\activate # Windows

# Install dependencies
pip install -r requirements.txt

# Now python and python3 point to 3.13 in this venv
python nlplc_llvm.py program.nlpl -o output
```

## Checking Your Python Version

```bash
# Check default Python 3
python3 --version

# Check specific version
python3.13 --version
python3.12 --version
python3.11 --version
```

## Why Not Python 3.14?

Python 3.14.0 (released 2024) has a regression in the import system that causes
infinite hangs when loading certain module patterns, including NLPL's lexer module.

**Symptoms**:
- Scripts hang on `from nexuslang.parser.lexer import Lexer` 
- No error message, just infinite loop
- Affects all NexusLang tools (compiler, tests, etc.)

**Status**: Bug reported to Python project, waiting for fix

**Workaround**: Use Python 3.13 or earlier

## Quick Start with Correct Python

```bash
# 1. Check version
python3 --version

# 2. If it shows 3.14, use specific version
python3.13 --version

# 3. Create alias (optional, add to ~/.bashrc)
alias nlpl-python=python3.13

# 4. Compile NexusLang programs
python3.13 nlplc_llvm.py examples/01_basic_concepts.nlpl -o test
./test

# 5. Run tests 
python3.13 -m pytest tests/
```

## Continuous Integration

For CI/CD pipelines, specify Python version explicitly:

```yaml
# GitHub Actions
- uses: actions/setup-python@v4
 with:
 python-version: '3.13'

# GitLab CI
image: python:3.13

# Docker
FROM python:3.13-slim
```

## Dependencies

NLPL requires:
- llvmlite >= 0.40.0
- pytest >= 7.0.0 (for testing)

These work with Python 3.11-3.13.

---

**Last Updated**: 2025-11-26 
**Issue**: Python 3.14 import regression blocking NexusLang development
