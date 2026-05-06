"""
NLPL - NexusLang

A truly universal programming language that combines natural English-like syntax
with low-level capabilities.
"""

__version__ = "0.1.0"
__author__ = "The No Hands Company"

# All imports are lazy to prevent circular import deadlocks caused by
# submodules (e.g. runtime.py) importing from nexuslang.runtime.memory which
# re-enters this __init__.py before it has finished executing.
# This is particularly problematic under Python 3.14's revised import system.

__all__ = [
    "Lexer",
    "Token",
    "TokenType",
    "Parser",
    "Interpreter",
    "Runtime",
]

def __getattr__(name: str):
    if name in ("Lexer", "Token", "TokenType"):
        from nexuslang.parser.lexer import Lexer, Token, TokenType  # noqa: PLC0415
        globals().update({"Lexer": Lexer, "Token": Token, "TokenType": TokenType})
        return globals()[name]
    if name == "Parser":
        from nexuslang.parser.parser import Parser  # noqa: PLC0415
        globals()["Parser"] = Parser
        return Parser
    if name == "Interpreter":
        from nexuslang.interpreter.interpreter import Interpreter  # noqa: PLC0415
        globals()["Interpreter"] = Interpreter
        return Interpreter
    if name == "Runtime":
        from nexuslang.runtime.runtime import Runtime  # noqa: PLC0415
        globals()["Runtime"] = Runtime
        return Runtime
    raise AttributeError(f"module 'nexuslang' has no attribute {name!r}")
