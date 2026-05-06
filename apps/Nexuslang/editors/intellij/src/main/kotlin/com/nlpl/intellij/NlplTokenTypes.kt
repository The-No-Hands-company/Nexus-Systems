package com.nlpl.intellij

import com.intellij.psi.tree.IElementType

/**
 * Token type singletons used by the NexusLang highlighting lexer.
 */
object NlplTokenTypes {
    @JvmField val KEYWORD      = IElementType("NLPL_KEYWORD",      NlplLanguage)
    @JvmField val BUILTIN      = IElementType("NLPL_BUILTIN",      NlplLanguage)
    @JvmField val TYPE         = IElementType("NLPL_TYPE",         NlplLanguage)
    @JvmField val CONSTANT     = IElementType("NLPL_CONSTANT",     NlplLanguage)
    @JvmField val STRING       = IElementType("NLPL_STRING",       NlplLanguage)
    @JvmField val NUMBER       = IElementType("NLPL_NUMBER",       NlplLanguage)
    @JvmField val COMMENT      = IElementType("NLPL_COMMENT",      NlplLanguage)
    @JvmField val DOC_COMMENT  = IElementType("NLPL_DOC_COMMENT",  NlplLanguage)
    @JvmField val IDENTIFIER   = IElementType("NLPL_IDENTIFIER",   NlplLanguage)
    @JvmField val OPERATOR     = IElementType("NLPL_OPERATOR",     NlplLanguage)
    @JvmField val WHITESPACE   = IElementType("NLPL_WHITESPACE",   NlplLanguage)
    @JvmField val OTHER        = IElementType("NLPL_OTHER",        NlplLanguage)
}
