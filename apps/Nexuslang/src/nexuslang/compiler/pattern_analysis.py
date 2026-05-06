"""
Pattern matching static analysis.
Provides exhaustiveness checking and unreachable case detection.
"""

from nexuslang.parser.ast import (
    LiteralPattern, IdentifierPattern, WildcardPattern,
    VariantPattern, TuplePattern, ListPattern
)


class PatternAnalyzer:
    """Analyzes pattern matching for completeness and redundancy."""
    
    def __init__(self):
        self.warnings = []
    
    def analyze_match(self, match_expr):
        """Analyze a match expression for exhaustiveness and unreachable cases.
        
        Returns:
            List of warnings (strings)
        """
        self.warnings = []
        
        # Check for exhaustiveness
        is_exhaustive = self._is_exhaustive(match_expr.cases)
        if not is_exhaustive:
            self.warnings.append(
                f"Non-exhaustive pattern match at line {match_expr.line_number}. "
                f"Missing patterns may cause runtime errors."
            )
        
        # Check for unreachable cases
        unreachable = self._find_unreachable_cases(match_expr.cases)
        for case_index in unreachable:
            pattern_type = type(match_expr.cases[case_index].pattern).__name__
            self.warnings.append(
                f"Unreachable case at case #{case_index + 1} (pattern: {pattern_type}). "
                f"This case will never match due to earlier patterns."
            )
        
        return self.warnings
    
    def _is_exhaustive(self, cases):
        """Check if pattern matching is exhaustive (covers all possible values)."""
        # Check for wildcard or identifier pattern (catches all)
        for case in cases:
            pattern = case.pattern
            if isinstance(pattern, (WildcardPattern, IdentifierPattern)):
                return True
        
        # For literal patterns, exhaustiveness depends on type
        # Integer/String patterns are never exhaustive without wildcard
        # Boolean patterns are exhaustive if both true and false are covered
        
        literal_values = set()
        for case in cases:
            if isinstance(case.pattern, LiteralPattern):
                if case.pattern.value.type == 'boolean':
                    literal_values.add(case.pattern.value.value)
        
        # Boolean exhaustiveness: need both true and false
        if len(literal_values) > 0:
            if True in literal_values and False in literal_values:
                return True
        
        # For variant patterns, check if all variants are covered
        variant_names = set()
        for case in cases:
            if isinstance(case.pattern, VariantPattern):
                variant_names.add(case.pattern.variant_name)
        
        # Result<T, E> exhaustiveness: need both Ok and Error/Err
        result_variants = {'Ok', 'Error', 'Err'}
        if len(variant_names & result_variants) >= 2:
            return True
        
        # Option<T> exhaustiveness: need both Some and None
        option_variants = {'Some', 'None'}
        if option_variants.issubset(variant_names):
            return True
        
        # Default: not exhaustive
        return False
    
    def _find_unreachable_cases(self, cases):
        """Find cases that are unreachable due to earlier patterns.
        
        Returns:
            List of case indices that are unreachable
        """
        unreachable = []
        
        for i, case in enumerate(cases):
            # Check if this case is shadowed by earlier cases
            for j in range(i):
                earlier_case = cases[j]
                
                # Wildcard or identifier patterns shadow everything after them
                if isinstance(earlier_case.pattern, (WildcardPattern, IdentifierPattern)):
                    if earlier_case.guard is None:  # Only if no guard
                        unreachable.append(i)
                        break
                
                # Duplicate literal patterns
                if isinstance(earlier_case.pattern, LiteralPattern) and \
                   isinstance(case.pattern, LiteralPattern):
                    if self._literals_equal(earlier_case.pattern.value, case.pattern.value):
                        if earlier_case.guard is None:  # Only if no guard
                            unreachable.append(i)
                            break
                
                # Same variant patterns
                if isinstance(earlier_case.pattern, VariantPattern) and \
                   isinstance(case.pattern, VariantPattern):
                    if earlier_case.pattern.variant_name == case.pattern.variant_name:
                        if earlier_case.guard is None:  # Only if no guard
                            unreachable.append(i)
                            break
        
        return unreachable
    
    def _literals_equal(self, lit1, lit2):
        """Check if two literals are equal."""
        return lit1.type == lit2.type and lit1.value == lit2.value


def analyze_pattern_match(match_expr):
    """Convenience function to analyze a match expression.
    
    Args:
        match_expr: MatchExpression AST node
    
    Returns:
        List of warning strings
    """
    analyzer = PatternAnalyzer()
    return analyzer.analyze_match(match_expr)
