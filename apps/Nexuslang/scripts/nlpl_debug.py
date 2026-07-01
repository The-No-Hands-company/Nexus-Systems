#!/usr/bin/env python3
"""
NLPL Debugger Entry Point

Convenience script to launch the NexusLang debugger.

Usage:
    python nxl_debug.py program.nlpl [--break 10 --break 25]
    
Or make executable and run directly:
    chmod +x nxl_debug.py
    ./nxl_debug.py program.nlpl
"""

import sys
import os

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from nexuslang.debugger.debugger import main

if __name__ == '__main__':
    main()
