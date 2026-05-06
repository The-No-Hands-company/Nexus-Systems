#!/usr/bin/env python3
"""
NexusLang Language Server Entry Point

Canonical launcher script expected by editor and docs.
"""

import os
import sys

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from nexuslang.lsp.server import NexusLangLanguageServer


def main():
    """Start the NexusLang Language Server."""
    server = NexusLangLanguageServer()
    server.start()


if __name__ == "__main__":
    main()
