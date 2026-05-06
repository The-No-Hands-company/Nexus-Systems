"""
NexusLang Debugger Integration
==============================

Enables source-level debugging of NexusLang programs.

Components:
- Interactive debugger (debugger.py)
- DWARF debug info generation (debug_info.py)
- Debug symbol table (symbols.py)
- Debug Adapter Protocol server (dap_server.py)
"""

from .debug_info import DebugInfoGenerator
from .symbols import SymbolTable, DebugSymbol, SymbolType
from .debugger import Debugger, DebuggerState, Breakpoint, CallFrame
from .dap_server import DAPServer

__all__ = [
    'DebugInfoGenerator',
    'SymbolTable',
    'DebugSymbol',
    'SymbolType',
    'Debugger',
    'DebuggerState',
    'Breakpoint',
    'CallFrame',
    'DAPServer',
]
