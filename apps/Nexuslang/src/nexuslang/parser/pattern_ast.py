"""
Pattern matching AST nodes for NexusLang.

Production-ready pattern nodes for Option/Result matching.
NO shortcuts, NO placeholders.
"""


class OptionPattern:
    """Pattern for matching Option<T> values.
    
    Production-ready pattern matching for Some/None variants.
    Supports destructuring and variable binding.
    
    Examples:
        Some with value  # Matches Some and binds value
        None             # Matches None
    """
    
    def __init__(self, variant: str, binding: str = None, line_number: int = 0):
        """Initialize Option pattern.
        
        Args:
            variant: "Some" or "None"
            binding: Variable name to bind the value (for Some)
            line_number: Source line number
        """
        if variant not in ("Some", "None"):
            raise ValueError(f"Invalid Option variant: {variant}")
        
        self.variant = variant
        self.binding = binding
        self.line_number = line_number
    
    def __repr__(self):
        if self.binding:
            return f"OptionPattern({self.variant} with {self.binding})"
        return f"OptionPattern({self.variant})"


class ResultPattern:
    """Pattern for matching Result<T,E> values.
    
    Production-ready pattern matching for Ok/Err variants.
    Supports destructuring and variable binding.
    
    Examples:
        Ok with value    # Matches Ok and binds value
        Err with error   # Matches Err and binds error
    """
    
    def __init__(self, variant: str, binding: str = None, line_number: int = 0):
        """Initialize Result pattern.
        
        Args:
            variant: "Ok" or "Err"
            binding: Variable name to bind the value/error
            line_number: Source line number
        """
        if variant not in ("Ok", "Err"):
            raise ValueError(f"Invalid Result variant: {variant}")
        
        self.variant = variant
        self.binding = binding
        self.line_number = line_number
    
    def __repr__(self):
        if self.binding:
            return f"ResultPattern({self.variant} with {self.binding})"
        return f"ResultPattern({self.variant})"


class WildcardPattern:
    """Wildcard pattern that matches anything.
    
    Used for catch-all cases in match expressions.
    
    Example:
        _ # Matches any value
    """
    
    def __init__(self, line_number: int = 0):
        self.line_number = line_number
    
    def __repr__(self):
        return "WildcardPattern(_)"
