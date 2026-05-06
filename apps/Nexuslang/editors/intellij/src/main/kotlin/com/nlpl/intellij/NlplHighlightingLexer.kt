package com.nlpl.intellij

import com.intellij.lexer.LexerBase
import com.intellij.psi.tree.IElementType

/**
 * A simple regex-based lexer for NexusLang syntax highlighting.
 * This is used only for initial token colouring; semantic enrichment
 * is provided by the LSP server at runtime.
 */
class NlplHighlightingLexer : LexerBase() {

    private var buffer: CharSequence = ""
    private var startOffset: Int = 0
    private var endOffset: Int = 0
    private var currentOffset: Int = 0
    private var tokenStart: Int = 0
    private var tokenEnd: Int = 0
    private var tokenType: IElementType? = null

    companion object {
        private val KEYWORDS = setOf(
            "set", "to", "function", "returns", "return", "if", "else", "otherwise",
            "while", "for", "each", "in", "end", "class", "extends", "struct", "union",
            "new", "import", "from", "export", "as", "with", "and", "or", "not",
            "is", "are", "has", "have", "do", "does", "be", "been", "being",
            "match", "case", "default", "try", "catch", "finally", "throw",
            "print", "read", "write", "allocate", "free", "address", "of",
            "dereference", "sizeof", "break", "continue", "pass", "yield",
            "async", "await", "lambda", "called", "defined", "a", "an", "the",
            "value", "type", "where", "when", "result", "output", "input",
            "greater", "less", "than", "equal", "greater than", "less than",
            "equal to", "not equal", "greater than or equal to", "less than or equal to",
            "plus", "minus", "times", "divided", "modulo", "power",
            "bitwise", "shift", "left", "right"
        )

        private val BUILTINS = setOf(
            "print_text", "println", "input", "len", "range", "map", "filter",
            "reduce", "sorted", "reversed", "enumerate", "zip", "sum", "min",
            "max", "abs", "round", "floor", "ceil", "sqrt", "sin", "cos",
            "tan", "log", "exp", "random", "randint", "choice", "shuffle",
            "read_file", "write_file", "append_file", "file_exists", "delete_file",
            "list_dir", "make_dir", "join_path", "split_path", "basename", "dirname",
            "format", "parse_int", "parse_float", "parse_bool", "to_string",
            "is_instance_of", "type_of", "hash", "id", "repr"
        )

        private val TYPES = setOf(
            "Integer", "Float", "String", "Boolean", "List", "Dict", "Tuple",
            "Set", "Optional", "Any", "Void", "Byte", "Short", "Long",
            "Double", "Char", "Array", "Map", "Queue", "Stack",
            "Result", "Error", "Function"
        )

        private val CONSTANTS = setOf("true", "false", "null", "nothing", "empty", "infinity")
    }

    override fun start(buffer: CharSequence, startOffset: Int, endOffset: Int, initialState: Int) {
        this.buffer = buffer
        this.startOffset = startOffset
        this.endOffset = endOffset
        this.currentOffset = startOffset
        advance()
    }

    override fun getState(): Int = 0

    override fun getTokenType(): IElementType? = tokenType

    override fun getTokenStart(): Int = tokenStart

    override fun getTokenEnd(): Int = tokenEnd

    override fun getBufferSequence(): CharSequence = buffer

    override fun getBufferEnd(): Int = endOffset

    override fun advance() {
        if (currentOffset >= endOffset) {
            tokenType = null
            return
        }

        tokenStart = currentOffset
        val ch = buffer[currentOffset]

        when {
            // Doc comment: ##
            ch == '#' && currentOffset + 1 < endOffset && buffer[currentOffset + 1] == '#' -> {
                scanToEol()
                tokenType = NlplTokenTypes.DOC_COMMENT
            }

            // Line comment: #
            ch == '#' -> {
                scanToEol()
                tokenType = NlplTokenTypes.COMMENT
            }

            // Triple-quoted string
            ch == '"' && currentOffset + 2 < endOffset
                    && buffer[currentOffset + 1] == '"'
                    && buffer[currentOffset + 2] == '"' -> {
                currentOffset += 3
                while (currentOffset + 2 < endOffset) {
                    if (buffer[currentOffset] == '"'
                        && buffer[currentOffset + 1] == '"'
                        && buffer[currentOffset + 2] == '"') {
                        currentOffset += 3
                        break
                    }
                    currentOffset++
                }
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.STRING
            }

            // Double-quoted string
            ch == '"' -> {
                currentOffset++
                while (currentOffset < endOffset && buffer[currentOffset] != '"' && buffer[currentOffset] != '\n') {
                    if (buffer[currentOffset] == '\\') currentOffset++
                    currentOffset++
                }
                if (currentOffset < endOffset && buffer[currentOffset] == '"') currentOffset++
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.STRING
            }

            // Single-quoted string
            ch == '\'' -> {
                currentOffset++
                while (currentOffset < endOffset && buffer[currentOffset] != '\'' && buffer[currentOffset] != '\n') {
                    if (buffer[currentOffset] == '\\') currentOffset++
                    currentOffset++
                }
                if (currentOffset < endOffset && buffer[currentOffset] == '\'') currentOffset++
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.STRING
            }

            // Number: integer or float
            ch.isDigit() || (ch == '-' && currentOffset + 1 < endOffset && buffer[currentOffset + 1].isDigit()) -> {
                if (ch == '-') currentOffset++
                while (currentOffset < endOffset && (buffer[currentOffset].isDigit() || buffer[currentOffset] == '.')) {
                    currentOffset++
                }
                // Scientific notation
                if (currentOffset < endOffset && (buffer[currentOffset] == 'e' || buffer[currentOffset] == 'E')) {
                    currentOffset++
                    if (currentOffset < endOffset && (buffer[currentOffset] == '+' || buffer[currentOffset] == '-')) currentOffset++
                    while (currentOffset < endOffset && buffer[currentOffset].isDigit()) currentOffset++
                }
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.NUMBER
            }

            // Identifier or keyword
            ch.isLetter() || ch == '_' -> {
                while (currentOffset < endOffset && (buffer[currentOffset].isLetterOrDigit() || buffer[currentOffset] == '_')) {
                    currentOffset++
                }
                tokenEnd = currentOffset
                val word = buffer.substring(tokenStart, tokenEnd)
                tokenType = when (word) {
                    in KEYWORDS  -> NlplTokenTypes.KEYWORD
                    in BUILTINS  -> NlplTokenTypes.BUILTIN
                    in TYPES     -> NlplTokenTypes.TYPE
                    in CONSTANTS -> NlplTokenTypes.CONSTANT
                    else         -> NlplTokenTypes.IDENTIFIER
                }
            }

            // Operators and punctuation
            ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '^'
                    || ch == '<' || ch == '>' || ch == '=' || ch == '!' || ch == '&'
                    || ch == '|' || ch == '~' -> {
                currentOffset++
                // Two-char operators
                if (currentOffset < endOffset) {
                    val next = buffer[currentOffset]
                    if ((ch == '<' && next == '=') || (ch == '>' && next == '=')
                        || (ch == '=' && next == '=') || (ch == '!' && next == '=')
                        || (ch == '<' && next == '<') || (ch == '>' && next == '>')
                        || (ch == '*' && next == '*') || (ch == '/' && next == '/')
                    ) {
                        currentOffset++
                    }
                }
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.OPERATOR
            }

            // Whitespace
            ch.isWhitespace() -> {
                while (currentOffset < endOffset && buffer[currentOffset].isWhitespace()) currentOffset++
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.WHITESPACE
            }

            else -> {
                currentOffset++
                tokenEnd = currentOffset
                tokenType = NlplTokenTypes.OTHER
            }
        }
    }

    private fun scanToEol() {
        while (currentOffset < endOffset && buffer[currentOffset] != '\n') currentOffset++
        tokenEnd = currentOffset
    }
}
