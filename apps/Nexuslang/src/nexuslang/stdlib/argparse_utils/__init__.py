"""
Argument parsing for NexusLang programs.
Command-line argument parsing utilities.
"""

import argparse
from typing import List, Dict, Any, Optional
from ...runtime.runtime import Runtime


class ArgumentParser:
    """Wrapper around argparse.ArgumentParser."""
    
    def __init__(self, description: str = "", prog: Optional[str] = None):
        self.parser = argparse.ArgumentParser(description=description, prog=prog)
        self.args = None
    
    def add_argument(self, name: str, help_text: str = "", 
                    arg_type: str = "str", default: Any = None,
                    required: bool = False, action: Optional[str] = None) -> None:
        """Add argument to parser."""
        type_map = {
            'str': str,
            'int': int,
            'float': float,
            'bool': bool
        }
        
        kwargs = {'help': help_text}
        
        if action:
            kwargs['action'] = action
        else:
            kwargs['type'] = type_map.get(arg_type, str)
        
        if default is not None:
            kwargs['default'] = default
        
        if required:
            kwargs['required'] = required
        
        self.parser.add_argument(name, **kwargs)
    
    def parse(self, args: Optional[List[str]] = None) -> Dict[str, Any]:
        """Parse arguments and return as dictionary."""
        self.args = self.parser.parse_args(args)
        return vars(self.args)
    
    def get_value(self, name: str) -> Any:
        """Get parsed argument value."""
        if self.args is None:
            return None
        clean_name = name.lstrip('-').replace('-', '_')
        return getattr(self.args, clean_name, None)


def create_parser(description: str = "", prog: Optional[str] = None) -> ArgumentParser:
    """Create argument parser."""
    return ArgumentParser(description=description, prog=prog)


def add_argument(parser: ArgumentParser, name: str, help_text: str = "",
                arg_type: str = "str", default: Any = None,
                required: bool = False, action: Optional[str] = None) -> ArgumentParser:
    """Add argument to parser (returns parser for chaining)."""
    parser.add_argument(name, help_text, arg_type, default, required, action)
    return parser


def parse_args(parser: ArgumentParser, args: Optional[List[str]] = None) -> Dict[str, Any]:
    """Parse arguments."""
    return parser.parse(args)


def get_arg_value(parser: ArgumentParser, name: str) -> Any:
    """Get argument value."""
    return parser.get_value(name)


def register_argparse_functions(runtime: Runtime) -> None:
    """Register argument parsing functions with the runtime."""
    
    # Parser creation and management
    runtime.register_function("create_parser", create_parser)
    runtime.register_function("add_argument", add_argument)
    runtime.register_function("parse_args", parse_args)
    runtime.register_function("get_arg_value", get_arg_value)
