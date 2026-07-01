#!/usr/bin/env python3
"""
NLPL REPL Entry Point

Convenience script to launch the NexusLang interactive REPL.

Usage:
    python nxl_repl.py [--debug] [--no-type-check]
    
Or make executable and run directly:
    chmod +x nxl_repl.py
    ./nxl_repl.py
"""

import sys
import os

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from nexuslang.repl.repl import main

if __name__ == '__main__':
    main()
