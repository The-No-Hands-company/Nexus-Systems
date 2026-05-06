grammar NLPL;

// Parser Rules
program
    : statement* EOF
    ;

statement
    : variableDeclaration
    | functionDefinition
    | ifStatement
    | whileLoop
    | forLoop
    | tryStatement
    | printStatement
    | returnStatement
    | appendStatement
    | fileOperation
    ;

variableDeclaration
    : SET identifier TO expression
    ;

expression
    : literal                                                # LiteralExpr
    | identifier                                            # IdentifierExpr
    | expression PLUS expression                            # AdditionExpr
    | expression MINUS expression                           # SubtractionExpr
    | expression TIMES expression                           # MultiplicationExpr
    | expression DIVIDED BY expression                      # DivisionExpr
    | expression IS GREATER THAN expression                 # GreaterThanExpr
    | expression IS LESS THAN expression                    # LessThanExpr
    | expression IS EQUAL TO expression                     # EqualityExpr
    | expression IS GREATER THAN OR EQUAL TO expression     # GreaterThanEqualExpr
    | expression IS LESS THAN OR EQUAL TO expression        # LessThanEqualExpr
    | expression IS NOT EQUAL TO expression                 # InequalityExpr
    | LENGTH OF identifier                                  # LengthExpr
    | CONVERT expression TO type                            # ConversionExpr
    | '[' (expression (',' expression)*)? ']'               # ListExpr
    | identifier '[' expression ']'                         # IndexExpr
    ;

literal
    : STRING_LITERAL
    | INTEGER_LITERAL
    | FLOAT_LITERAL
    | TRUE
    | FALSE
    ;

type
    : IDENTIFIER
    | LIST OF type
    ;

identifier
    : IDENTIFIER
    ;

// Lexer Rules
SET : 'set' ;
TO : 'to' ;
IS : 'is' ;
GREATER : 'greater' ;
LESS : 'less' ;
THAN : 'than' ;
EQUAL : 'equal' ;
NOT : 'not' ;
OR : 'or' ;
PLUS : 'plus' ;
MINUS : 'minus' ;
TIMES : 'times' ;
DIVIDED : 'divided' ;
BY : 'by' ;
LENGTH : 'length' ;
OF : 'of' ;
CONVERT : 'convert' ;
LIST : 'List' ;
TRUE : 'true' ;
FALSE : 'false' ;

IDENTIFIER : [a-zA-Z_][a-zA-Z0-9_]* ;
INTEGER_LITERAL : [0-9]+ ;
FLOAT_LITERAL : [0-9]+ '.' [0-9]+ ;
STRING_LITERAL : '"' (~["\r\n])* '"' ;

WS : [ \t\r\n]+ -> skip ;
COMMENT : '#' ~[\r\n]* -> skip ; 