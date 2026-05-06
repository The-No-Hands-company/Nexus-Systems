package com.nlpl.intellij

import com.intellij.lang.Language

/**
 * Singleton Language descriptor for NexusLang.
 * Registered as an IntelliJ Platform Language extension point.
 */
object NlplLanguage : Language("NexusLang") {

    override fun getDisplayName(): String = "NexusLang"

    override fun isCaseSensitive(): Boolean = true

    override fun getMimeTypes(): Array<String> = arrayOf("text/x-nlpl", "application/x-nlpl")

    override fun getAssociatedFileType() = NlplFileType
}
