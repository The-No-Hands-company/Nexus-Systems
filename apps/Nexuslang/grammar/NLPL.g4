/**
 * NexusLang Grammar — March 2026
 *
 * ANTLR4 grammar for the NexusLang.
 * This grammar reflects the actual parser implemented in
 * src/nlpl/parser/parser.py (9 700+ lines, hand-written recursive descent).
 *
 * Design note:
 *   NexusLang uses structured English as its surface syntax.  Keywords like
 *   'and', 'or', 'to', 'with', 'as', 'of' serve dual roles as both
 *   logical/structural connectives and as prose-style flow words.
 *   Certain ambiguities (e.g. 'and' in parameter lists vs. logical 'and')
 *   are resolved by the hand-written parser via context; they cannot be
 *   expressed unambiguously in pure context-free grammar.  Such spots are
 *   marked with a // CONTEXT comment below.
 *
 * Compatibility note:
 *   This grammar is an authoritative reference document.  The primary
 *   implementation artefact is the Python parser, not a generated ANTLR
 *   parser.  Always treat the Python parser as the ground truth for
 *   edge-case behaviour.
 */

grammar NLPL;

// ==========================================================================
// Top-level program
// ==========================================================================

program
    : statement* EOF
    ;


// ==========================================================================
// Statements
// ==========================================================================

statement
    : variableDeclaration
    | assignment
    | indexAssignment
    | memberAssignment
    | functionDefinition
    | classDefinition
    | interfaceDefinition
    | traitDefinition
    | structDefinition
    | unionDefinition
    | enumDefinition
    | ifStatement
    | whileLoop
    | repeatNTimes
    | repeatWhileLoop
    | forEachLoop
    | parallelForLoop
    | switchStatement
    | matchStatement
    | returnStatement
    | breakStatement
    | continueStatement
    | printStatement
    | appendStatement
    | panicStatement
    | fallthroughStatement
    | sendStatement
    | tryCatch
    | raiseStatement
    | assertStatement
    | requireStatement
    | ensureStatement
    | guaranteeStatement
    | invariantStatement
    | memoryAllocation
    | memoryDeallocation
    | freeStatement
    | inlineAssembly
    | unsafeBlock
    | foreignFunction
    | externDeclaration
    | typeAlias
    | importStatement
    | exportStatement
    | testBlock
    | asyncStatement
    | awaitStatement
    | spawnStatement
    | expressionStatement
    ;

expressionStatement
    : expression
    ;

// --------------------------------------------------------------------------
// Variable declaration
//
//   set x to 42
//   set name to "Alice" as String
//   set x to 0 as Integer
// --------------------------------------------------------------------------

variableDeclaration
    : SET IDENTIFIER TO expression AS typeAnnotation
    | SET IDENTIFIER TO expression
    | SET IDENTIFIER AS typeAnnotation TO expression
    ;

// --------------------------------------------------------------------------
// Assignment (re-assignment of existing variable)
//
//   set x to x plus 1
// --------------------------------------------------------------------------

assignment
    : SET IDENTIFIER TO expression
    ;

// --------------------------------------------------------------------------
// Index assignment:   set arr[i] to value
// --------------------------------------------------------------------------

indexAssignment
    : SET expression '[' expression ']' TO expression
    ;

// --------------------------------------------------------------------------
// Member assignment
//
//   set obj.field to value
//   set field of obj to value
// --------------------------------------------------------------------------

memberAssignment
    : SET expression '.' IDENTIFIER TO expression
    | SET IDENTIFIER OF expression TO expression
    ;

// --------------------------------------------------------------------------
// Function definition
//
//   function greet
//       print text "hello"
//   end
//
//   function add with a as Integer and b as Integer returns Integer
//       return a plus b
//   end
//
//   async function fetch with url as String returns String
//       ...
//   end
//
//   function process with items as List of Integer and *rest as String
//       ...
//   end
//
//   Generator declaration pattern (parser-aligned):
//   function sequence with n as Integer returns List of Integer
//       yield 0
//       yield n
//   end
//
//   Notes:
//   - The parser identifies generator functions by `yield` usage in the body.
//   - Return annotations for generators are conventionally `List of T`.
// --------------------------------------------------------------------------

functionDefinition
    : ASYNC? FUNCTION IDENTIFIER genericTypeParams?
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
        contractClauses?
        functionBody
        END
    ;

parameterList
    : parameter (AND parameter)*       // CONTEXT: 'and' is a separator here
    ;

parameter
    : VARIADIC? IDENTIFIER AS typeAnnotation (DEFAULT TO expression)?
    | VARIADIC? IDENTIFIER (DEFAULT TO expression)?
    ;

VARIADIC : '*' ;

genericTypeParams
    : '<' genericParam (',' genericParam)* '>'
    ;

genericParam
    : IDENTIFIER (WHERE IDENTIFIER IS typeConstraintList)?
    | IDENTIFIER '::' kindAnnotation
    ;

kindAnnotation
    : '*'
    | '*' ARROW '*'
    | '(' kindAnnotation ')' ARROW kindAnnotation
    ;

ARROW : '->' ;

typeConstraintList
    : IDENTIFIER (AND IDENTIFIER)*
    ;

functionBody
    : statement*
    ;

contractClauses
    : contractClause+
    ;

contractClause
    : REQUIRE expression
    | ENSURE expression
    | GUARANTEE expression
    | INVARIANT expression
    ;

// --------------------------------------------------------------------------
// Class definition
//
//   class Animal
//       name as String
//       function speak
//           print text "..."
//       end
//   end
//
//   class Dog extends Animal implements Printable
//       function speak
//           print text "Woof"
//       end
//   end
// --------------------------------------------------------------------------

classDefinition
    : CLASS IDENTIFIER genericTypeParams?
        (EXTENDS typeList)?
        (IMPLEMENTS typeList)?
        classBody
        END
    ;

classBody
    : classMember*
    ;

classMember
    : classMemberModifier* (propertyDeclaration
    | methodDefinition
    | operatorMethodDefinition
    | constructorDefinition)
    ;

classMemberModifier
    : PUBLIC
    | PRIVATE
    | PROTECTED
    | STATIC
    | ABSTRACT
    ;

propertyDeclaration
    : PROPERTY IDENTIFIER AS typeAnnotation (DEFAULT TO expression)?
    | IDENTIFIER AS typeAnnotation (DEFAULT TO expression)?
    ;

methodDefinition
    : functionDefinition
    | ABSTRACT FUNCTION IDENTIFIER genericTypeParams?
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
    ;

operatorMethodDefinition
    : OPERATOR operatorSymbol
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
        functionBody
        END
    ;

operatorSymbol
    : '+'
    | '-'
    | '*'
    | '/'
    | '%'
    | '**'
    | '^'
    | EQUALS
    | NOT EQUALS
    | '<'
    | '>'
    | '<='
    | '>='
    | 'plus'
    | MINUS
    | TIMES
    | DIVIDED BY
    | MODULO
    ;

constructorDefinition
    : CONSTRUCTOR (WITH parameterList)?
        functionBody
        END
    ;

// --------------------------------------------------------------------------
// Interface definition
//
//   interface Printable
//       function to_string returns String
//   end
// --------------------------------------------------------------------------

interfaceDefinition
    : INTERFACE IDENTIFIER genericTypeParams?
        (EXTENDS typeList)?
        interfaceBody
        END
    ;

interfaceBody
    : methodSignature*
    ;

methodSignature
    : FUNCTION IDENTIFIER genericTypeParams?
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
    ;

// --------------------------------------------------------------------------
// Trait definition
//
//   trait Functor
//       function map with f as function returns Self
//   end
//
//   trait Monad requires Applicative
//       function bind with f as function returns Self
//       function unit with value returns Self
//   end
// --------------------------------------------------------------------------

traitDefinition
    : TRAIT IDENTIFIER genericTypeParams?
        (REQUIRES typeList)?
        traitBody
        END
    ;

traitBody
    : (methodDefinition | methodSignature)*
    ;

// --------------------------------------------------------------------------
// Struct / union / enum
//
//   struct Point
//       x as Integer
//       y as Integer
//   end
//
//   union Shape
//       circle as Float
//       rectangle as Float
//   end
//
//   enum Direction
//       North
//       South = 2
//   end
// --------------------------------------------------------------------------

structDefinition
    : STRUCT IDENTIFIER
        structField*
        END
    ;

structField
    : IDENTIFIER AS typeAnnotation
    ;

unionDefinition
    : UNION IDENTIFIER
        unionVariant*
        END
    ;

unionVariant
    : IDENTIFIER AS typeAnnotation
    | IDENTIFIER
    ;

enumDefinition
    : ENUM IDENTIFIER
        enumVariant*
        END
    ;

enumVariant
    : IDENTIFIER ('=' expression)?
    ;

// --------------------------------------------------------------------------
// Control flow
// --------------------------------------------------------------------------

ifStatement
    : IF expression
        statement*
        elseIfClause*
        elseClause?
      END
    ;

elseIfClause
    : ELSE IF expression
        statement*
    ;

elseClause
    : ELSE
        statement*
    ;

whileLoop
        : labelPrefix? WHILE expression
        statement*
      END
    ;

repeatNTimes
    : REPEAT expression TIMES
        statement*
      END
    ;

repeatWhileLoop
    : REPEAT WHILE expression
        statement*
      END
    ;

forEachLoop
        : labelPrefix? FOR EACH IDENTIFIER (WITH INDEX IDENTIFIER)? IN expression
        statement*
      END
    ;

parallelForLoop
        : PARALLEL FOR EACH IDENTIFIER IN expression
                statement*
            END
        | PARALLEL FOR IDENTIFIER IN expression
                statement*
            END
        ;

labelPrefix
    : LABEL IDENTIFIER ':'
    ;

switchStatement
    : SWITCH expression
        switchCase+
        defaultCase?
    ;

switchCase
    : CASE expression
        statement*
    ;

defaultCase
    : DEFAULT
        statement*
    ;

// --------------------------------------------------------------------------
// Pattern matching
//
//   match value with
//       case 0
//           print text "zero"
//       case x when x is greater than 0
//           print text x
//       case _
//           print text "other"
//   end
// --------------------------------------------------------------------------

matchStatement
    : MATCH expression WITH
        matchCase*
      END
    ;

matchCase
    : CASE pattern (WHEN expression)?
        statement*
    ;

pattern
    : literal
    | IDENTIFIER
    | '_'
    | '[' (pattern (',' pattern)*)? ']'
    | '(' pattern ',' pattern (',' pattern)* ')'
    | IDENTIFIER '(' (pattern (',' pattern)*)? ')'
    | structPattern
    ;

structPattern
    : IDENTIFIER '{' fieldPattern (',' fieldPattern)* '}'
    ;

fieldPattern
    : IDENTIFIER ':' pattern
    | IDENTIFIER
    ;

// --------------------------------------------------------------------------
// Return / print / append
// --------------------------------------------------------------------------

returnStatement
    : RETURN expression
    | RETURN
    ;

breakStatement
    : BREAK IDENTIFIER?
    ;

continueStatement
    : CONTINUE IDENTIFIER?
    ;

panicStatement
    : PANIC (WITH expression)?
    ;

fallthroughStatement
    : FALLTHROUGH
    ;

printStatement
    : PRINT TEXT expression
    | PRINT TEXT NEWLINE_KW
    ;

sendStatement
    : SEND expression TO expression
    ;

appendStatement
    : APPEND expression TO expression
    ;

// --------------------------------------------------------------------------
// Exception handling
//
//   try
//       risky_operation
//   catch err
//       print text err
//   end
// --------------------------------------------------------------------------

tryCatch
    : TRY
        statement*
      CATCH IDENTIFIER
        (WITH IDENTIFIER (COMMA IDENTIFIER)*)?
        (AS IDENTIFIER)?
        statement*
      (FINALLY statement*)?
      END
    ;

raiseStatement
    : RAISE (IDENTIFIER (WITH IDENTIFIER expression)?)?
    | THROW (IDENTIFIER (WITH IDENTIFIER expression)?)?
    ;

assertStatement
    : ASSERT expression
    | ASSERT expression COMMA expression
    ;

requireStatement  : REQUIRE expression ;
ensureStatement   : ENSURE expression ;
guaranteeStatement: GUARANTEE expression ;
invariantStatement: INVARIANT expression ;

// --------------------------------------------------------------------------
// Memory management (low-level)
//
//   allocate buffer as Integer
//   allocate 1024 bytes
//   deallocate ptr
//   free buffer
// --------------------------------------------------------------------------

memoryAllocation
    : ALLOCATE IDENTIFIER AS typeAnnotation
    | ALLOCATE expression BYTES
    ;

memoryDeallocation
    : DEALLOCATE expression
    ;

freeStatement
    : FREE expression
    ;

// --------------------------------------------------------------------------
// Inline assembly
//
//   asm
//       mov rax, 60
//       xor rdi, rdi
//       syscall
//   end
// --------------------------------------------------------------------------

inlineAssembly
    : ASM asmBody END
    ;

asmBody
    : ( ~END_KW )*     // raw token sequence until 'end' keyword
    ;

// --------------------------------------------------------------------------
// Foreign Function Interface
//
//   foreign function puts from "libc" with s as String returns Integer
// --------------------------------------------------------------------------

foreignFunction
    : FOREIGN FUNCTION IDENTIFIER
        (FROM STRING_LITERAL)?
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
        END?
    ;

externDeclaration
    : EXTERN FUNCTION IDENTIFIER
        (WITH parameterList)?
        (RETURNS typeAnnotation)?
        (FROM LIBRARY STRING_LITERAL)?
    | EXTERN IDENTIFIER IDENTIFIER AS typeAnnotation
        (FROM LIBRARY STRING_LITERAL)?
    | EXTERN TYPE IDENTIFIER AS typeAnnotation
    ;

// --------------------------------------------------------------------------
// Type alias
//
//   type Callback as function with x as Integer returns Boolean
// --------------------------------------------------------------------------

typeAlias
    : TYPE IDENTIFIER AS typeAnnotation
    ;

// --------------------------------------------------------------------------
// Module system
//
//   import math
//   import math as m
//   import only sqrt and pow from math
//   export function compute
// --------------------------------------------------------------------------

importStatement
    : IMPORT IDENTIFIER (AS IDENTIFIER)?
    | IMPORT ONLY importNameList FROM IDENTIFIER
    ;

importNameList
    : IDENTIFIER (AND IDENTIFIER)*
    ;

exportStatement
    : EXPORT statement
    | EXPORT IDENTIFIER
    ;

// --------------------------------------------------------------------------
// Test blocks (built-in test runner)
//
//   test "addition works"
//       assert 1 plus 1 equals 2
//   end
// --------------------------------------------------------------------------

testBlock
    : TEST STRING_LITERAL
        statement*
      END
    ;

// --------------------------------------------------------------------------
// Async / concurrency
// --------------------------------------------------------------------------

asyncStatement : ASYNC statement ;
awaitStatement : AWAIT expression ;
spawnStatement : SPAWN expression ;

unsafeBlock
    : UNSAFE DO? statement* END
    ;


// ==========================================================================
// Expressions
// ==========================================================================

expression
    : coalesce
    ;

coalesce
    : logicalOr (OTHERWISE logicalOr)*
    ;

logicalOr
    : logicalAnd (OR logicalAnd)*
    ;

logicalAnd
    : logicalNot (AND logicalNot)*      // CONTEXT: 'and' is logical operator
    ;

logicalNot
    : NOT logicalNot
    | equality
    ;

equality
    : comparison (equalityOp comparison)*
    ;

equalityOp
    : EQUALS
    | NOT EQUALS
    | IS NOT
    | IS
    ;

comparison
    : membership (compOp membership)?
    ;

compOp
    : IS LESS THAN OR EQUAL TO
    | IS GREATER THAN OR EQUAL TO
    | IS LESS THAN
    | IS GREATER THAN
    | '<='
    | '>='
    | '<'
    | '>'
    ;

membership
    : bitwiseOr ((IN | NOT IN) bitwiseOr)?
    | bitwiseOr IS EMPTY
    | bitwiseOr IS NOT EMPTY
    | bitwiseOr
    ;

bitwiseOr
    : bitwiseXor (BITWISE_OR bitwiseXor)*
    ;

bitwiseXor
    : bitwiseAnd (BITWISE_XOR bitwiseAnd)*
    ;

bitwiseAnd
    : bitwiseShift (BITWISE_AND bitwiseShift)*
    ;

bitwiseShift
    : addition ((LEFT SHIFT | RIGHT SHIFT) addition)*
    ;

addition
    : multiplication ((PLUS | MINUS) multiplication)*
    ;

multiplication
    : unary ((TIMES | DIVIDED BY | MODULO) unary)*
    ;

unary
    : MINUS unary
    | BITWISE_NOT unary
    | NOT unary
    | power
    ;

power
    : postfix (TO THE POWER OF unary)?
    | postfix ('^' unary)?
    ;

postfix
    : atom
    | postfix '[' expression ']'
    | postfix '.' IDENTIFIER
    | postfix OF postfix
    ;

atom
    : literal
    | IDENTIFIER
    | generatorExpression
    | '(' expression ')'
    | listLiteral
    | dictLiteral
    | tupleLiteral
    | listComprehension
    | dictComprehension
    | lambdaExpression
    | castExpression
    | sizeofExpression
    | addressOfExpression
    | dereferenceExpression
    | lengthExpression
    | genericTypeInstantiation
    | receiveExpression
    | yieldExpression
    | functionCall
    | AWAIT expression
    ;

receiveExpression
    : RECEIVE FROM? expression
    ;

yieldExpression
    : YIELD expression?
    ;

generatorExpression
    : '(' expression FOR expression IN expression (IF expression)? ')'
    ;

functionCall
    : IDENTIFIER (WITH namedArgList)?
    | IDENTIFIER (WITH namedArgList)? DO trailingBlock END
    | IDENTIFIER DO trailingBlock END
    ;

namedArgList
    : namedArg (AND namedArg)*
    ;

namedArg
    : IDENTIFIER ':' expression
    | expression
    ;

trailingBlock
    : (IDENTIFIER AS typeAnnotation)? statement*
    ;

listLiteral
    : '[' (expression (',' expression)*)? ']'
    ;

dictLiteral
    : '{' (dictEntry (',' dictEntry)*)? '}'
    ;

dictEntry
    : expression ':' expression
    ;

tupleLiteral
    : '(' expression ',' expression (',' expression)* ')'
    ;

listComprehension
    : '[' expression FOR EACH IDENTIFIER IN expression (IF expression)? ']'
    ;

dictComprehension
    : '{' expression ':' expression FOR EACH IDENTIFIER IN expression (IF expression)? '}'
    ;

lambdaExpression
    : DO (IDENTIFIER AS typeAnnotation)? statement* END
    ;

castExpression
    : CONVERT expression TO typeAnnotation
    ;

sizeofExpression
    : SIZEOF expression
    | SIZEOF TYPE typeAnnotation
    ;

addressOfExpression
    : ADDRESS OF expression
    ;

dereferenceExpression
    : DEREFERENCE expression
    ;

lengthExpression
    : LENGTH OF expression
    ;

genericTypeInstantiation
    : CREATE IDENTIFIER OF typeAnnotation
    | CREATE IDENTIFIER WITH LENGTH expression
    | CREATE ARRAY OF typeAnnotation WITH LENGTH expression
    | CREATE CHANNEL
    ;


// ==========================================================================
// Type annotations
// ==========================================================================

typeAnnotation
    : IDENTIFIER                                                    // Integer
    | IDENTIFIER OF typeAnnotation                                  // List of Integer
    | IDENTIFIER OF typeAnnotation TO typeAnnotation                // Dict of String to Integer
    | FUNCTION (WITH paramTypeList)? (RETURNS typeAnnotation)?      // function type
    | typeAnnotation '?'                                            // optional
    | typeAnnotation '*'                                            // pointer
    | '(' typeAnnotation (',' typeAnnotation)+ ')'                 // tuple type
    | typeAnnotation '|' typeAnnotation                             // union type
    | IDENTIFIER '<' typeAnnotation (',' typeAnnotation)* '>'      // generic: List<T>
    ;

paramTypeList
    : typeAnnotation (AND typeAnnotation)*
    ;

typeList
    : IDENTIFIER (AND IDENTIFIER)*
    ;


// ==========================================================================
// Literals
// ==========================================================================

literal
    : INTEGER_LITERAL
    | FLOAT_LITERAL
    | STRING_LITERAL
    | FSTRING_LITERAL
    | BOOLEAN_LITERAL
    | NULL
    ;


// ==========================================================================
// Lexer rules — keywords (alphabetical within category)
// ==========================================================================

// Declaration
ABSTRACT   : 'abstract' ;
ADDRESS    : 'address' ;
ALLOCATE   : 'allocate' ;
AND        : 'and' ;
APPEND     : 'append' ;
ARRAY      : 'array' ;
AS         : 'as' ;
ASSERT     : 'assert' ;
ASYNC      : 'async' ;
AWAIT      : 'await' ;
BITWISE_AND: 'bitwise' WS 'and' ;
BITWISE_NOT: 'bitwise' WS 'not' ;
BITWISE_OR : 'bitwise' WS 'or' ;
BITWISE_XOR: 'bitwise' WS 'xor' ;
BYTES      : 'bytes' ;
BY         : 'by' ;
BREAK      : 'break' ;
CASE       : 'case' ;
CATCH      : 'catch' ;
CLASS      : 'class' ;
CHANNEL    : 'channel' ;
COMMA      : ',' ;
CONSTRUCTOR: 'constructor' ;
CONTINUE   : 'continue' ;
CONVERT    : 'convert' ;
CREATE     : 'create' ;
DEALLOCATE : 'deallocate' ;
DEFAULT    : 'default' ;
DEREFERENCE: 'dereference' ;
DIVIDED    : 'divided' ;
DO         : 'do' ;
EACH       : 'each' ;
ELSE       : 'else' ;
EMPTY      : 'empty' ;
END        : 'end' ;
END_KW     : 'end' ;     // alias used in asmBody rule
ENSURE     : 'ensure' ;
ENUM       : 'enum' ;
EQUALS     : 'equals' ;
EXTERN     : 'extern' ;
EXPORT     : 'export' ;
EXTENDS    : 'extends' ;
FALLTHROUGH: 'fallthrough' ;
FINALLY    : 'finally' ;
FOR        : 'for' ;
FOREIGN    : 'foreign' ;
FREE       : 'free' ;
FROM       : 'from' ;
FUNCTION   : 'function' ;
GUARANTEE  : 'guarantee' ;
IF         : 'if' ;
IMPLEMENTS : 'implements' ;
IMPORT     : 'import' ;
IN         : 'in' ;
INDEX      : 'index' ;
INTERFACE  : 'interface' ;
INVARIANT  : 'invariant' ;
IS         : 'is' ;
LABEL      : 'label' ;
LEFT       : 'left' ;
LENGTH     : 'length' ;
LESS       : 'less' ;
LIBRARY    : 'library' ;
GREATER    : 'greater' ;
MATCH      : 'match' ;
MINUS      : 'minus' ;
MODULO     : 'modulo' ;
NEW        : 'new' ;
NEWLINE_KW : 'newline' ;
NOT        : 'not' ;
NULL       : 'null' ;
OF         : 'of' ;
ONLY       : 'only' ;
OPERATOR   : 'operator' ;
OR         : 'or' ;
OTHERWISE  : 'otherwise' ;
PANIC      : 'panic' ;
PLUS       : 'plus' ;
POWER      : 'power' ;
PRINT      : 'print' ;
PRIVATE    : 'private' ;
PROPERTY   : 'property' ;
PROTECTED  : 'protected' ;
PUBLIC     : 'public' ;
RAISE      : 'raise' ;
RECEIVE    : 'receive' ;
REPEAT     : 'repeat' ;
REQUIRE    : 'require' ;
REQUIRES   : 'requires' ;
RETURN     : 'return' ;
RETURNS    : 'returns' ;
RIGHT      : 'right' ;
SEND       : 'send' ;
SET        : 'set' ;
SHIFT      : 'shift' ;
SIZEOF     : 'sizeof' ;
SPAWN      : 'spawn' ;
STATIC     : 'static' ;
STRUCT     : 'struct' ;
SWITCH     : 'switch' ;
TEST       : 'test' ;
TEXT       : 'text' ;
THE        : 'the' ;
THAN       : 'than' ;
THROW      : 'throw' ;
TIMES      : 'times' ;
TO         : 'to' ;
TRAIT      : 'trait' ;
TRY        : 'try' ;
TYPE       : 'type' ;
UNION      : 'union' ;
UNSAFE     : 'unsafe' ;
WHEN       : 'when' ;
WHERE      : 'where' ;
WHILE      : 'while' ;
WITH       : 'with' ;
YIELD      : 'yield' ;


// ==========================================================================
// Lexer rules — literals and identifiers
// ==========================================================================

BOOLEAN_LITERAL
    : 'true' | 'false'
    ;

INTEGER_LITERAL
    : [0-9]+
    | '0x' [0-9a-fA-F]+
    | '0b' [01]+
    | '0o' [0-7]+
    ;

FLOAT_LITERAL
    : [0-9]+ '.' [0-9]+ ([eE] [+-]? [0-9]+)?
    ;

STRING_LITERAL
    : '"'  ( ESC_SEQ | ~["\\\r\n]  )* '"'
    | '\'' ( ESC_SEQ | ~['\\\r\n] )* '\''
    ;

FSTRING_LITERAL
    : 'f"'  ( ESC_SEQ | FSTRING_EXPR | ~["\\\r\n]  )* '"'
    | 'f\'' ( ESC_SEQ | FSTRING_EXPR | ~['\\\r\n] )* '\''
    ;

fragment FSTRING_EXPR : '{' ~[{}]* '}' ;
fragment ESC_SEQ      : '\\' ( ["\\'ntr0bu] ) ;

// Identifiers — ASCII plus common Latin-extended and Greek/Cyrillic blocks
IDENTIFIER
    : [a-zA-Z_\u00C0-\u024F\u0370-\u03FF\u0400-\u04FF]
      [a-zA-Z0-9_\u00C0-\u024F\u0370-\u03FF\u0400-\u04FF]*
    ;

COMMENT
    : '#' ~[\r\n]* -> skip
    ;

WS
    : [ \t\r\n]+ -> skip
    ;
COMMENT: '#' ~[\r\n]* -> skip; 