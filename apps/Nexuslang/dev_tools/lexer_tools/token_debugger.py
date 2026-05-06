"""
NLPL Development Tools - Lexer Debugging Utilities
===================================================

Tools for inspecting and debugging the lexer/tokenization phase.
"""

import sys
import os
from typing import List, Optional
from colorama import init, Fore, Back, Style

# Add project root to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))

from src.parser.lexer import Lexer, Token, TokenType

# Initialize colorama for cross-platform colored output
init(autoreset=True)


class TokenVisualizer:
    """Visualize tokens with color coding and formatting."""
    
    TOKEN_COLORS = {
        # Keywords
        'SET': Fore.CYAN,
        'FUNCTION': Fore.CYAN,
        'CLASS': Fore.CYAN,
        'IF': Fore.CYAN,
        'WHILE': Fore.CYAN,
        'FOR': Fore.CYAN,
        
        # Literals
        'STRING_LITERAL': Fore.GREEN,
        'INTEGER_LITERAL': Fore.YELLOW,
        'FLOAT_LITERAL': Fore.YELLOW,
        'TRUE': Fore.MAGENTA,
        'FALSE': Fore.MAGENTA,
        
        # Identifiers
        'IDENTIFIER': Fore.WHITE,
        
        # Operators
        'PLUS': Fore.RED,
        'MINUS': Fore.RED,
        'TIMES': Fore.RED,
        'DIVIDED_BY': Fore.RED,
        
        # Punctuation
        'LEFT_PAREN': Fore.BLUE,
        'RIGHT_PAREN': Fore.BLUE,
        'LEFT_BRACKET': Fore.BLUE,
        'RIGHT_BRACKET': Fore.BLUE,
    }
    
    @staticmethod
    def visualize_tokens(tokens: List[Token], show_line_numbers: bool = True) -> None:
        """Display tokens with color coding and formatting."""
        print(f"\n{Style.BRIGHT}{'='*80}")
        print(f"TOKEN VISUALIZATION ({len(tokens)} tokens)")
        print(f"{'='*80}{Style.RESET_ALL}\n")
        
        current_line = None
        line_tokens = []
        
        for token in tokens:
            if token.type == TokenType.EOF:
                continue
                
            # Print line header when we encounter a new line
            if show_line_numbers and token.line != current_line:
                if line_tokens:
                    print()  # Newline after previous line's tokens
                current_line = token.line
                print(f"{Fore.LIGHTBLACK_EX}Line {current_line:3d}:{Style.RESET_ALL} ", end='')
                line_tokens = []
            
            # Get color for token type
            token_type_name = token.type.name
            color = TokenVisualizer.TOKEN_COLORS.get(token_type_name, Fore.WHITE)
            
            # Display token
            display_value = token.lexeme if hasattr(token, 'lexeme') else str(token.literal)
            print(f"{color}{display_value}{Style.RESET_ALL}", end=' ')
            line_tokens.append(token)
        
        print("\n")
    
    @staticmethod
    def dump_token_details(tokens: List[Token], output_file: Optional[str] = None) -> None:
        """Dump detailed token information to console or file."""
        output = []
        output.append(f"\n{'='*100}")
        output.append(f"DETAILED TOKEN DUMP ({len(tokens)} tokens)")
        output.append(f"{'='*100}\n")
        output.append(f"{'#':<5} {'Line':<6} {'Col':<5} {'Type':<25} {'Lexeme':<30} {'Literal':<20}")
        output.append(f"{'-'*100}")
        
        for i, token in enumerate(tokens):
            if token.type == TokenType.EOF:
                continue
                
            lexeme = token.lexeme if hasattr(token, 'lexeme') else ''
            literal = str(token.literal) if hasattr(token, 'literal') and token.literal is not None else ''
            
            # Truncate long values
            lexeme = (lexeme[:27] + '...') if len(lexeme) > 30 else lexeme
            literal = (literal[:17] + '...') if len(literal) > 20 else literal
            
            output.append(
                f"{i:<5} {token.line:<6} {token.column:<5} {token.type.name:<25} "
                f"{lexeme:<30} {literal:<20}"
            )
        
        output_text = '\n'.join(output)
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(output_text)
            print(f"Token details dumped to: {output_file}")
        else:
            print(output_text)


class TokenStatistics:
    """Analyze and report statistics about tokenized code."""
    
    @staticmethod
    def analyze(tokens: List[Token]) -> dict:
        """Analyze token statistics."""
        stats = {
            'total_tokens': len(tokens),
            'by_type': {},
            'lines': set(),
            'keywords': 0,
            'literals': 0,
            'identifiers': 0,
            'operators': 0,
        }
        
        keyword_types = {TokenType.SET, TokenType.FUNCTION, TokenType.CLASS, TokenType.IF, 
                        TokenType.WHILE, TokenType.FOR}
        literal_types = {TokenType.STRING_LITERAL, TokenType.INTEGER_LITERAL, 
                        TokenType.FLOAT_LITERAL, TokenType.TRUE, TokenType.FALSE}
        operator_types = {TokenType.PLUS, TokenType.MINUS, TokenType.TIMES, 
                         TokenType.DIVIDED_BY, TokenType.IS, TokenType.EQUAL_TO}
        
        for token in tokens:
            if token.type == TokenType.EOF:
                continue
                
            # Count by type
            type_name = token.type.name
            stats['by_type'][type_name] = stats['by_type'].get(type_name, 0) + 1
            
            # Track lines
            stats['lines'].add(token.line)
            
            # Categorize
            if token.type in keyword_types:
                stats['keywords'] += 1
            elif token.type in literal_types:
                stats['literals'] += 1
            elif token.type == TokenType.IDENTIFIER:
                stats['identifiers'] += 1
            elif token.type in operator_types:
                stats['operators'] += 1
        
        stats['total_lines'] = len(stats['lines'])
        del stats['lines']  # Remove set from stats
        
        return stats
    
    @staticmethod
    def print_statistics(stats: dict) -> None:
        """Print formatted statistics."""
        print(f"\n{Style.BRIGHT}{'='*80}")
        print(f"TOKEN STATISTICS")
        print(f"{'='*80}{Style.RESET_ALL}\n")
        
        print(f"Total Tokens:    {stats['total_tokens']}")
        print(f"Total Lines:     {stats['total_lines']}")
        print(f"Keywords:        {stats['keywords']}")
        print(f"Identifiers:     {stats['identifiers']}")
        print(f"Literals:        {stats['literals']}")
        print(f"Operators:       {stats['operators']}")
        
        print(f"\n{Style.BRIGHT}Token Type Distribution:{Style.RESET_ALL}")
        sorted_types = sorted(stats['by_type'].items(), key=lambda x: x[1], reverse=True)
        for token_type, count in sorted_types:
            bar = '' * min(50, count)
            print(f"  {token_type:<25} {count:4d} {Fore.CYAN}{bar}{Style.RESET_ALL}")


class KeywordChecker:
    """Check for keyword conflicts and coverage."""
    
    @staticmethod
    def check_keywords() -> None:
        """Display all registered keywords and check for issues."""
        print(f"\n{Style.BRIGHT}{'='*80}")
        print(f"KEYWORD REGISTRY CHECK")
        print(f"{'='*80}{Style.RESET_ALL}\n")
        
        # Get all TokenTypes
        all_types = [t for t in TokenType]
        
        print(f"Total TokenTypes defined: {len(all_types)}\n")
        
        # Categorize
        categories = {
            'Keywords': [],
            'Operators': [],
            'Literals': [],
            'Control Flow': [],
            'Data Types': [],
            'Other': []
        }
        
        for token_type in all_types:
            name = token_type.name
            if any(kw in name for kw in ['SET', 'FUNCTION', 'CLASS', 'DEFINE', 'IMPORT']):
                categories['Keywords'].append(name)
            elif any(op in name for op in ['PLUS', 'MINUS', 'EQUAL', 'GREATER', 'LESS']):
                categories['Operators'].append(name)
            elif 'LITERAL' in name or name in ['TRUE', 'FALSE']:
                categories['Literals'].append(name)
            elif any(cf in name for cf in ['IF', 'WHILE', 'FOR', 'BREAK']):
                categories['Control Flow'].append(name)
            elif any(dt in name for dt in ['INTEGER', 'FLOAT', 'STRING', 'BOOLEAN']):
                categories['Data Types'].append(name)
            else:
                categories['Other'].append(name)
        
        for category, tokens in categories.items():
            if tokens:
                print(f"{Style.BRIGHT}{category}:{Style.RESET_ALL} ({len(tokens)})")
                for i, token in enumerate(sorted(tokens), 1):
                    print(f"  {i:3d}. {token}")
                print()


def main():
    """Main entry point for lexer debugging tools."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NLPL Lexer Debugging Tools')
    parser.add_argument('file', nargs='?', help='NLPL file to analyze')
    parser.add_argument('-v', '--visualize', action='store_true', help='Visualize tokens with colors')
    parser.add_argument('-d', '--dump', action='store_true', help='Dump detailed token information')
    parser.add_argument('-s', '--stats', action='store_true', help='Show token statistics')
    parser.add_argument('-k', '--keywords', action='store_true', help='Check keyword registry')
    parser.add_argument('-o', '--output', help='Output file for dump')
    parser.add_argument('--no-line-numbers', action='store_true', help='Hide line numbers in visualization')
    
    args = parser.parse_args()
    
    # If checking keywords, don't need a file
    if args.keywords:
        KeywordChecker.check_keywords()
        if not args.file:
            return
    
    if not args.file:
        parser.print_help()
        return
    
    # Read the file
    try:
        with open(args.file, 'r') as f:
            source_code = f.read()
    except FileNotFoundError:
        print(f"{Fore.RED}Error: File '{args.file}' not found{Style.RESET_ALL}")
        return
    
    # Tokenize
    print(f"\n{Fore.CYAN}Tokenizing: {args.file}{Style.RESET_ALL}")
    try:
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
    except Exception as e:
        print(f"{Fore.RED}Lexer Error: {e}{Style.RESET_ALL}")
        return
    
    # Run requested tools
    if args.visualize or (not args.dump and not args.stats):
        TokenVisualizer.visualize_tokens(tokens, show_line_numbers=not args.no_line_numbers)
    
    if args.dump:
        TokenVisualizer.dump_token_details(tokens, args.output)
    
    if args.stats:
        stats = TokenStatistics.analyze(tokens)
        TokenStatistics.print_statistics(stats)


if __name__ == '__main__':
    main()
