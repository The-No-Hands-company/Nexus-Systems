package com.nlpl.intellij

import com.intellij.lexer.Lexer
import com.intellij.openapi.editor.DefaultLanguageHighlighterColors
import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.fileTypes.SyntaxHighlighterBase
import com.intellij.psi.tree.IElementType

/**
 * Syntax highlighter for NexusLang files.
 * Delegates tokenization to NlplLexer and maps token types to editor colors.
 */
class NlplSyntaxHighlighter : SyntaxHighlighterBase() {

    companion object {
        // Highlight attribute keys
        val KEYWORD    = TextAttributesKey.createTextAttributesKey("NLPL_KEYWORD",    DefaultLanguageHighlighterColors.KEYWORD)
        val STRING     = TextAttributesKey.createTextAttributesKey("NLPL_STRING",     DefaultLanguageHighlighterColors.STRING)
        val NUMBER     = TextAttributesKey.createTextAttributesKey("NLPL_NUMBER",     DefaultLanguageHighlighterColors.NUMBER)
        val COMMENT    = TextAttributesKey.createTextAttributesKey("NLPL_COMMENT",    DefaultLanguageHighlighterColors.LINE_COMMENT)
        val DOC_COMMENT= TextAttributesKey.createTextAttributesKey("NLPL_DOC_COMMENT",DefaultLanguageHighlighterColors.DOC_COMMENT)
        val IDENTIFIER = TextAttributesKey.createTextAttributesKey("NLPL_IDENTIFIER", DefaultLanguageHighlighterColors.IDENTIFIER)
        val FUNCTION   = TextAttributesKey.createTextAttributesKey("NLPL_FUNCTION",   DefaultLanguageHighlighterColors.FUNCTION_DECLARATION)
        val CLASS_NAME = TextAttributesKey.createTextAttributesKey("NLPL_CLASS",      DefaultLanguageHighlighterColors.CLASS_NAME)
        val OPERATOR   = TextAttributesKey.createTextAttributesKey("NLPL_OPERATOR",   DefaultLanguageHighlighterColors.OPERATION_SIGN)
        val BUILTIN    = TextAttributesKey.createTextAttributesKey("NLPL_BUILTIN",    DefaultLanguageHighlighterColors.PREDEFINED_SYMBOL)
        val TYPE       = TextAttributesKey.createTextAttributesKey("NLPL_TYPE",       DefaultLanguageHighlighterColors.CLASS_REFERENCE)
        val CONSTANT   = TextAttributesKey.createTextAttributesKey("NLPL_CONSTANT",   DefaultLanguageHighlighterColors.CONSTANT)

        private val KEYWORD_KEYS     = arrayOf(KEYWORD)
        private val STRING_KEYS      = arrayOf(STRING)
        private val NUMBER_KEYS      = arrayOf(NUMBER)
        private val COMMENT_KEYS     = arrayOf(COMMENT)
        private val DOC_COMMENT_KEYS = arrayOf(DOC_COMMENT)
        private val IDENTIFIER_KEYS  = arrayOf(IDENTIFIER)
        private val FUNCTION_KEYS    = arrayOf(FUNCTION)
        private val CLASS_KEYS       = arrayOf(CLASS_NAME)
        private val OPERATOR_KEYS    = arrayOf(OPERATOR)
        private val BUILTIN_KEYS     = arrayOf(BUILTIN)
        private val TYPE_KEYS        = arrayOf(TYPE)
        private val CONSTANT_KEYS    = arrayOf(CONSTANT)
        private val EMPTY            = emptyArray<TextAttributesKey>()
    }

    override fun getHighlightingLexer(): Lexer = NlplHighlightingLexer()

    override fun getTokenHighlights(tokenType: IElementType): Array<TextAttributesKey> {
        return when (tokenType) {
            NlplTokenTypes.KEYWORD     -> KEYWORD_KEYS
            NlplTokenTypes.BUILTIN     -> BUILTIN_KEYS
            NlplTokenTypes.TYPE        -> TYPE_KEYS
            NlplTokenTypes.CONSTANT    -> CONSTANT_KEYS
            NlplTokenTypes.STRING      -> STRING_KEYS
            NlplTokenTypes.NUMBER      -> NUMBER_KEYS
            NlplTokenTypes.COMMENT     -> COMMENT_KEYS
            NlplTokenTypes.DOC_COMMENT -> DOC_COMMENT_KEYS
            NlplTokenTypes.IDENTIFIER  -> IDENTIFIER_KEYS
            NlplTokenTypes.OPERATOR    -> OPERATOR_KEYS
            else -> EMPTY
        }
    }
}
