package com.nlpl.intellij

import com.intellij.openapi.fileTypes.LanguageFileType
import javax.swing.Icon

/**
 * Registers the .nlpl file type with IntelliJ Platform.
 */
object NlplFileType : LanguageFileType(NlplLanguage) {

    override fun getName(): String = "NLPL File"

    override fun getDescription(): String = "NLPL source file"

    override fun getDefaultExtension(): String = "nlpl"

    override fun getIcon(): Icon = NlplIcons.FILE
}
