"""
Workaround for Python 3.14 import hang issue.
Loads modules using exec() instead of import.
"""

import sys
import os
from types import ModuleType

def load_module_exec(module_path, module_name):
    """Load a module using exec() to avoid Python 3.14 import hang."""
    with open(module_path) as f:
        code = f.read()
    
    # Create a module object
    module = ModuleType(module_name)
    module.__file__ = module_path
    module.__name__ = module_name
    
    # Execute the code in the module's namespace
    exec(code, module.__dict__)
    
    # Add to sys.modules
    sys.modules[module_name] = module
    
    return module


def setup_nxl_workaround():
    """Setup NexusLang modules with workaround."""
    base_path = '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NexusLang/src'
    
    # Load lexer
    lexer = load_module_exec(
        os.path.join(base_path, 'nlpl/parser/lexer.py'),
        'nexuslang.parser.lexer'
    )
    
    # Make it available
    return lexer


if __name__ == '__main__':
    lexer_module = setup_nxl_workaround()
    print(f" Loaded lexer: {lexer_module.Lexer}")
    print(f" TokenType enum: {len(list(lexer_module.TokenType))} tokens")
    
    # Test it
    code = "set x to 5"
    lex = lexer_module.Lexer(code)
    tokens = list(lex.tokenize())
    print(f" Tokenized '{code}': {len(tokens)} tokens")
    for tok in tokens:
        print(f"   {tok.type.name}: {repr(tok.lexeme)}")
