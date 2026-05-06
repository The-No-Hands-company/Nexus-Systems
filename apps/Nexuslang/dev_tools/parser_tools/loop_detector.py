#!/usr/bin/env python3
"""
Infinite Loop Detector
======================

Detects when parser/interpreter gets stuck on the same token/state.
This would have caught the error_recovery() infinite loop immediately!

Monitors:
1. Token position changes during parsing
2. Repeated error_recovery() calls on same token
3. Statement parsing stuck on same position
4. Maximum iteration limits

Usage:
    python loop_detector.py <file.nlpl>
    python loop_detector.py <file.nlpl> --max-iterations 1000
    python loop_detector.py <file.nlpl> --strict
"""

import sys
import os
from pathlib import Path
from collections import defaultdict
from colorama import Fore, Style, init

init(autoreset=True)

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from src.parser.lexer import Lexer, TokenType
from src.parser.parser import Parser


class LoopDetector:
    """Detect infinite loops during parsing."""
    
    def __init__(self, max_iterations=100, strict=False):
        self.max_iterations = max_iterations
        self.strict = strict
        
        # Tracking
        self.token_positions = []
        self.error_recovery_calls = []
        self.same_position_count = defaultdict(int)
        self.iteration_count = 0
        
        # Detection thresholds
        self.same_position_threshold = 3 if strict else 10
        self.error_recovery_threshold = 2 if strict else 5
    
    def instrument_parser(self, parser):
        """Wrap parser methods to detect loops."""
        # Save original methods
        original_advance = parser.advance
        original_error_recovery = parser.error_recovery
        original_statement = parser.statement
        
        def tracked_advance():
            """Track token position changes."""
            before_token = parser.current_token
            before_pos = parser.current_token_index
            
            result = original_advance()
            
            after_token = parser.current_token
            after_pos = parser.current_token_index
            
            # Record position
            self.token_positions.append({
                'iteration': self.iteration_count,
                'before_pos': before_pos,
                'after_pos': after_pos,
                'before_token': before_token.type if before_token else None,
                'after_token': after_token.type if after_token else None
            })
            
            # Check if stuck on same position
            if before_pos == after_pos and before_token:
                position_key = f"{before_pos}:{before_token.type}"
                self.same_position_count[position_key] += 1
                
                if self.same_position_count[position_key] > self.same_position_threshold:
                    self._report_stuck_position(before_pos, before_token)
                    raise RuntimeError(f"Infinite loop detected: Stuck at position {before_pos}")
            
            return result
        
        def tracked_error_recovery():
            """Track error recovery calls."""
            token = parser.current_token
            pos = parser.current_token_index
            
            self.error_recovery_calls.append({
                'iteration': self.iteration_count,
                'position': pos,
                'token': token.type if token else None
            })
            
            # Check if error_recovery called repeatedly on same position
            recent_calls = [
                call for call in self.error_recovery_calls[-10:]
                if call['position'] == pos
            ]
            
            if len(recent_calls) > self.error_recovery_threshold:
                self._report_error_recovery_loop(pos, token)
                raise RuntimeError(f"Infinite loop in error_recovery at position {pos}")
            
            return original_error_recovery()
        
        def tracked_statement():
            """Track statement() method calls."""
            self.iteration_count += 1
            before_pos = parser.current_token_index
            
            # Check max iterations
            if self.iteration_count > self.max_iterations:
                self._report_max_iterations()
                raise RuntimeError(f"Max iterations ({self.max_iterations}) exceeded")
            
            # Call original statement
            try:
                result = original_statement()
            except Exception as e:
                # Track where error occurred
                after_pos = parser.current_token_index
                self.token_positions.append({
                    'iteration': self.iteration_count,
                    'before_pos': before_pos,
                    'after_pos': after_pos,
                    'error': str(e)
                })
                raise
            
            # Track position change
            after_pos = parser.current_token_index
            self.token_positions.append({
                'iteration': self.iteration_count,
                'before_pos': before_pos,
                'after_pos': after_pos
            })
            
            # Check if position changed
            if before_pos == after_pos:
                position_key = f"{before_pos}"
                self.same_position_count[position_key] += 1
                
                if self.same_position_count[position_key] > self.same_position_threshold:
                    token = parser.tokens[before_pos] if before_pos < len(parser.tokens) else None
                    self._report_stuck_position(before_pos, token)
                    raise RuntimeError(f"Infinite loop: statement() stuck at position {before_pos}")
            
            return result
        
        # Replace methods
        parser.advance = tracked_advance
        parser.error_recovery = tracked_error_recovery
        parser.statement = tracked_statement
        
        return parser
    
    def _report_stuck_position(self, position, token):
        """Report stuck on same position."""
        print(f"\n{Fore.RED}{'='*80}")
        print(f" INFINITE LOOP DETECTED: STUCK ON SAME TOKEN")
        print(f"{'='*80}\n")
        
        print(f"{Fore.YELLOW}Position: {position}")
        print(f"{Fore.YELLOW}Token: {token.type if token else 'None'}")
        print(f"{Fore.YELLOW}Stuck count: {self.same_position_count[f'{position}:{token.type}']}\n")
        
        print(f"{Fore.RED}The parser is repeatedly trying to process the same token")
        print(f"{Fore.RED}without making any forward progress.\n")
        
        print(f"{Fore.CYAN}Likely causes:")
        print(f"  1. error_recovery() not advancing past the problematic token")
        print(f"  2. statement() returning None without consuming token")
        print(f"  3. Missing handler for this token type\n")
        
        print(f"{Fore.GREEN}Recent position history:")
        for entry in self.token_positions[-10:]:
            print(f"  {entry['before_pos']} → {entry['after_pos']} "
                  f"({entry['before_token']} → {entry['after_token']})")
    
    def _report_error_recovery_loop(self, position, token):
        """Report error_recovery loop."""
        print(f"\n{Fore.RED}{'='*80}")
        print(f" INFINITE LOOP DETECTED: ERROR_RECOVERY LOOP")
        print(f"{'='*80}\n")
        
        print(f"{Fore.YELLOW}Position: {position}")
        print(f"{Fore.YELLOW}Token: {token.type if token else 'None'}\n")
        
        print(f"{Fore.RED}error_recovery() has been called {len(self.error_recovery_calls)}")
        print(f"{Fore.RED}times on the same position without making progress.\n")
        
        print(f"{Fore.CYAN}This is EXACTLY what happened with the PRINT token bug!")
        print(f"{Fore.CYAN}error_recovery() found PRINT as a statement boundary but")
        print(f"{Fore.CYAN}statement() had no handler, so it kept looping.\n")
        
        print(f"{Fore.GREEN}Recent error_recovery calls:")
        for call in self.error_recovery_calls[-5:]:
            print(f"  Iteration {call['iteration']}: pos={call['position']}, token={call['token']}")
    
    def _report_max_iterations(self):
        """Report max iterations exceeded."""
        print(f"\n{Fore.RED}{'='*80}")
        print(f" INFINITE LOOP DETECTED: MAX ITERATIONS EXCEEDED")
        print(f"{'='*80}\n")
        
        print(f"{Fore.YELLOW}Maximum iterations: {self.max_iterations}")
        print(f"{Fore.YELLOW}Actual iterations: {self.iteration_count}\n")
        
        print(f"{Fore.RED}The parser has processed more statements than expected.")
        print(f"{Fore.RED}This usually indicates an infinite loop.\n")


def main():
    import argparse
    
    parser_args = argparse.ArgumentParser(
        description="Detect infinite loops during parsing"
    )
    parser_args.add_argument(
        'file',
        help="NLPL source file to parse"
    )
    parser_args.add_argument(
        '--max-iterations',
        type=int,
        default=100,
        help="Maximum statement iterations (default: 100)"
    )
    parser_args.add_argument(
        '--strict',
        action='store_true',
        help="Use stricter thresholds for loop detection"
    )
    
    args = parser_args.parse_args()
    
    if not os.path.exists(args.file):
        print(f"{Fore.RED}Error: File not found: {args.file}")
        return 1
    
    # Read source
    with open(args.file, 'r') as f:
        source = f.read()
    
    print(f"{Fore.CYAN}{'='*80}")
    print(f"INFINITE LOOP DETECTION: {args.file}")
    print(f"{'='*80}\n")
    
    print(f"{Fore.YELLOW}Max iterations: {args.max_iterations}")
    print(f"{Fore.YELLOW}Strict mode: {args.strict}\n")
    
    # Tokenize
    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        print(f"{Fore.GREEN} Lexer: {len(tokens)} tokens\n")
    except Exception as e:
        print(f"{Fore.RED} Lexer error: {e}")
        return 1
    
    # Parse with loop detection
    try:
        parser = Parser(tokens)
        
        # Instrument parser
        detector = LoopDetector(
            max_iterations=args.max_iterations,
            strict=args.strict
        )
        parser = detector.instrument_parser(parser)
        
        print(f"{Fore.CYAN}Starting parse with loop detection...\n")
        
        ast = parser.parse()
        
        print(f"\n{Fore.GREEN}{'='*80}")
        print(f" PARSING COMPLETED SUCCESSFULLY")
        print(f"{'='*80}\n")
        
        print(f"  Statements parsed: {len(ast.statements)}")
        print(f"  Total iterations: {detector.iteration_count}")
        print(f"  Token advances: {len(detector.token_positions)}")
        print(f"  Error recoveries: {len(detector.error_recovery_calls)}\n")
        
        if detector.error_recovery_calls:
            print(f"{Fore.YELLOW} Warning: Error recovery was called {len(detector.error_recovery_calls)} times")
            print(f"{Fore.YELLOW}  This may indicate syntax errors or incomplete handlers\n")
        
        print(f"{Fore.GREEN} No infinite loops detected!\n")
        return 0
        
    except RuntimeError as e:
        if "Infinite loop" in str(e):
            print(f"\n{Fore.RED}{'='*80}")
            print(f"DIAGNOSIS: {e}")
            print(f"{'='*80}\n")
            return 1
        raise
    
    except Exception as e:
        print(f"\n{Fore.RED} Parser error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
