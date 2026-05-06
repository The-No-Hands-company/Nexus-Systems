"""
Suggestion Engine
=================

Provides "did you mean?" suggestions using fuzzy matching and heuristics.
"""

from typing import List, Optional, Set
from difflib import SequenceMatcher


class SuggestionEngine:
    """
    Generate helpful suggestions for common errors.
    
    Uses:
    - Fuzzy string matching (Levenshtein distance)
    - Common typo patterns
    - Context-aware suggestions
    """
    
    def __init__(self, similarity_threshold: float = 0.6):
        self.similarity_threshold = similarity_threshold
        self.common_typos = {
            # Common variable name typos
            'nmae': 'name',
            'lenght': 'length',
            'wdith': 'width',
            'heigth': 'height',
            'retrun': 'return',
            'fucntion': 'function',
            'calss': 'class',
        }
    
    def suggest_similar(self, target: str, candidates: List[str], max_suggestions: int = 3) -> List[str]:
        """
        Find similar strings from candidates.
        
        Args:
            target: The misspelled word
            candidates: Possible correct spellings
            max_suggestions: Maximum number of suggestions
            
        Returns:
            List of suggestions, sorted by similarity
        """
        # Check common typos first
        if target in self.common_typos:
            correction = self.common_typos[target]
            if correction in candidates:
                return [correction]
        
        # Calculate similarity scores
        similarities = []
        for candidate in candidates:
            score = self._similarity(target, candidate)
            if score >= self.similarity_threshold:
                similarities.append((score, candidate))
        
        # Sort by similarity (descending)
        similarities.sort(reverse=True, key=lambda x: x[0])
        
        # Return top suggestions
        return [candidate for _, candidate in similarities[:max_suggestions]]
    
    def suggest_for_undefined_variable(self, var_name: str, available_vars: Set[str]) -> Optional[str]:
        """
        Suggest correction for undefined variable.
        
        Returns:
            Suggestion message or None
        """
        if not available_vars:
            return None
        
        suggestions = self.suggest_similar(var_name, list(available_vars))
        
        if not suggestions:
            return f"available variables: {', '.join(sorted(available_vars)[:5])}"
        
        if len(suggestions) == 1:
            return f"did you mean '{suggestions[0]}'?"
        else:
            quoted = [f"'{s}'" for s in suggestions]
            return f"did you mean one of: {', '.join(quoted)}?"
    
    def suggest_for_undefined_function(self, func_name: str, available_funcs: Set[str]) -> Optional[str]:
        """Suggest correction for undefined function."""
        if not available_funcs:
            return None
        
        suggestions = self.suggest_similar(func_name, list(available_funcs))
        
        if not suggestions:
            return None
        
        if len(suggestions) == 1:
            return f"did you mean '{suggestions[0]}'?"
        else:
            quoted = [f"'{s}'" for s in suggestions]
            return f"did you mean one of: {', '.join(quoted)}?"
    
    def suggest_for_type_error(self, expected: str, actual: str) -> Optional[str]:
        """Suggest fix for type mismatch."""
        suggestions = []
        
        # Integer to Float
        if expected == "Float" and actual == "Integer":
            suggestions.append("convert to float: to_float with value")
        
        # String to Integer
        elif expected == "Integer" and actual == "String":
            suggestions.append("parse string: parse_int with text")
        
        # Integer to String
        elif expected == "String" and actual == "Integer":
            suggestions.append("convert to string: to_string with number")
        
        # List to Array
        elif expected.startswith("Array") and actual.startswith("List"):
            suggestions.append("convert to array: to_array with list")
        
        return suggestions[0] if suggestions else None
    
    def suggest_for_missing_import(self, symbol: str) -> List[str]:
        """Suggest imports for missing symbols."""
        # Common standard library modules
        stdlib_symbols = {
            'sqrt': 'math',
            'sin': 'math',
            'cos': 'math',
            'floor': 'math',
            'ceil': 'math',
            'abs': 'math',
            'max': 'math',
            'min': 'math',
            'split': 'string',
            'join': 'string',
            'trim': 'string',
            'replace': 'string',
            'read_file': 'io',
            'write_file': 'io',
            'print': 'io',
            'input': 'io',
        }
        
        if symbol in stdlib_symbols:
            module = stdlib_symbols[symbol]
            return [f"add import: import {module}"]
        
        return []
    
    def _similarity(self, a: str, b: str) -> float:
        """
        Calculate similarity between two strings.
        
        Uses SequenceMatcher (similar to Levenshtein distance).
        
        Returns:
            Similarity score from 0.0 to 1.0
        """
        # Case-insensitive comparison
        a_lower = a.lower()
        b_lower = b.lower()
        
        # Exact match bonus
        if a_lower == b_lower:
            return 1.0
        
        # Calculate base similarity
        similarity = SequenceMatcher(None, a_lower, b_lower).ratio()
        
        # Boost for same starting characters
        if a_lower and b_lower and a_lower[0] == b_lower[0]:
            similarity += 0.1
        
        # Boost for same length
        if len(a) == len(b):
            similarity += 0.05
        
        return min(similarity, 1.0)
    
    def generate_fix_it(self, error_type: str, **kwargs) -> Optional[str]:
        """
        Generate a fix-it suggestion.
        
        Args:
            error_type: Type of error
            **kwargs: Context-specific arguments
            
        Returns:
            Fix-it suggestion or None
        """
        if error_type == "missing_semicolon":
            return "add ';' at end of line"
        
        elif error_type == "unclosed_string":
            return "add closing quote"
        
        elif error_type == "unclosed_paren":
            return "add closing parenthesis ')'"
        
        elif error_type == "unclosed_bracket":
            return "add closing bracket ']'"
        
        elif error_type == "unclosed_brace":
            return "add closing brace '}'"
        
        elif error_type == "missing_colon":
            return "add ':' after function/class declaration"
        
        elif error_type == "wrong_keyword":
            correct = kwargs.get('correct')
            if correct:
                return f"use '{correct}' instead"
        
        return None


__all__ = ['SuggestionEngine']
